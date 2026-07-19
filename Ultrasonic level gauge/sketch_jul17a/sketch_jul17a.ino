/*
 * Ultrasonic level gauge — Arduino Nano 3
 * OLED 0.96" 128x64 I2C (SSD1306) + JSN-SR04T
 *
 * OLED init — как в рабочем примере Proteus (ssd1306_128x64_i2c):
 *   Adafruit_SSD1306 display(OLED_RESET);  RES=D4, адрес 0x3D
 *
 * Датчик UART (Proteus): TX датчика -> D0/PD0/RX0, RX датчика -> D1/PD1/TX0
 *   В Proteus SoftSerial часто НЕ работает — используем Hardware Serial (как в примере ET).
 * Датчик TRIG/ECHO (запас): TRIG=D9 (PB1), ECHO=D10 (PB2)
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// РЕЖИМ ДАТЧИКА — выбери один вариант
// =============================================================================
// Вариант A: TRIG/ECHO — закомментируй UART ниже и раскомментируй эту строку
 //#define SENSOR_MODE_TRIG_ECHO

// Вариант B (сейчас активен): UART — Proteus Electronics Tree HCSR04_UART / JSN-SR04T
#define SENSOR_MODE_UART

#define TRIG_PIN 9
#define ECHO_PIN 10

// --- UART порт ---
// 1 = Hardware Serial D0/D1 (PD0/PD1) — ДЛЯ PROTEUS (рекомендуется, как в примере ET)
// 0 = SoftwareSerial D6/D7 — запас для железа, если D0/D1 заняты USB-отладкой
#define SENSOR_UART_HW 1

#if defined(SENSOR_MODE_UART) && (SENSOR_UART_HW == 0)
#include <SoftwareSerial.h>
#define SENSOR_RX 6   // PD6 <- TX датчика
#define SENSOR_TX 7   // PD7 -> RX датчика
SoftwareSerial sensorSerial(SENSOR_RX, SENSOR_TX);
#define SENSOR_PORT sensorSerial
#elif defined(SENSOR_MODE_UART)
#define SENSOR_PORT Serial
#endif

// Proteus ET UART: расстояние в кадре в СМ. Реальный JSN часто мм → поставь 1.
#define SENSOR_DIST_IS_MM 0

// 0 = принимать кадр FF H L x даже если checksum не сошёлся (диагностика Proteus)
// 1 = только с верной контрольной суммой
#define SENSOR_UART_STRICT_CHECKSUM 0

// Период запроса (мс) для MODE=MANUAL. В AUTO кадр идёт только при ИЗМЕНЕНИИ setpoint!
const unsigned long SENSOR_POLL_MS = 200UL;

// =============================================================================
// OLED — как в рабочем примере Proteus (RES=D4, адрес 0x3D)
// =============================================================================
#define OLED_RESET 4                 // RES дисплея -> D4
#define OLED_ADDR  0x3D              // Proteus 128x64: 0x3D (не 0x3C)

// Как в примере Proteus: display(OLED_RESET). В .h библиотеки должен быть SSD1306_128_64.
Adafruit_SSD1306 display(OLED_RESET);

// Старая: WHITE/BLACK. Новая: SSD1306_WHITE.
#ifndef WHITE
#define WHITE SSD1306_WHITE
#endif
#ifndef BLACK
#define BLACK SSD1306_BLACK
#endif

// =============================================================================
// ПАРАМЕТРЫ РЕАЛЬНОЙ ЁМКОСТИ (калибровка)
// =============================================================================
const float TANK_CAPACITY_M3 = 12.0f;
const float TANK_HEIGHT_M    = 1.8f;    // <<< КАЛИБРОВКА: дно → горло, м (= 180 см)
const float TANK_AREA_M2     = TANK_CAPACITY_M3 / TANK_HEIGHT_M;
const float SENSOR_MOUNT_OFFSET_M = 0.0f;

// Как в Proteus на датчике: MAX = 180 см (пустая ёмкость). Совпадает с TANK_HEIGHT_M.
const float SENSOR_MAX_CM = TANK_HEIGHT_M * 100.0f;  // 180
const float SENSOR_MIN_CM = 0.0f;                    // «полная» / вплотную к зеркалу

float volumeFromLevel_m3(float level_m) {
  if (level_m < 0.0f) level_m = 0.0f;
  if (level_m > TANK_HEIGHT_M) level_m = TANK_HEIGHT_M;
  return level_m * TANK_AREA_M2;
}

float levelFromVolume_m(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;
  return volume_m3 / TANK_AREA_M2;
}

float levelFromDistance_m(float distance_m) {
  float level = TANK_HEIGHT_M - (distance_m - SENSOR_MOUNT_OFFSET_M);
  if (level < 0.0f) level = 0.0f;
  if (level > TANK_HEIGHT_M) level = TANK_HEIGHT_M;
  return level;
}

// =============================================================================
// Геометрия UI на OLED 128x64 — отступы 4 px от всех краёв
// =============================================================================
const int MARGIN = 4;
const int SCREEN_W = 128;
const int SCREEN_H = 64;

// Справа: подпись «12» (~12 px) + зазор + засечка 4 px
const int SCALE_LABEL_W = 12;
const int SCALE_TICK_W  = 4;
const int SCALE_GAP     = 2;   // между засечкой и цифрой
const int TANK_SCALE_GAP = 3;  // между стенкой ёмкости и засечками

const int SCALE_LABEL_X = SCREEN_W - MARGIN - SCALE_LABEL_W;          // 112
const int SCALE_X       = SCALE_LABEL_X - SCALE_GAP - SCALE_TICK_W;   // 106

const int TANK_X      = MARGIN;                                       // 4
const int TANK_Y      = MARGIN;                                       // 4
const int TANK_RIGHT  = SCALE_X - TANK_SCALE_GAP;                     // 103
const int TANK_W      = TANK_RIGHT - TANK_X;                           // 99
const int TANK_BOTTOM = SCREEN_H - MARGIN;                            // 60
const int TANK_H      = TANK_BOTTOM - TANK_Y;                          // 56

float currentLevel_m = 0.0f;       // пока нет кадра с датчика — пустая
float lastDistance_cm = SENSOR_MAX_CM;
bool  sensorHasReading = false;
unsigned int sensorBytesRx = 0;    // сколько байт дошло по UART (диагностика)
unsigned int sensorFramesOk = 0;   // сколько кадров принято
uint8_t lastRx[4] = {0, 0, 0, 0}; // последние 4 байта (для hex на экране)

void drawTankFrame() {
  display.drawFastVLine(TANK_X, TANK_Y, TANK_H, WHITE);
  display.drawFastHLine(TANK_X, TANK_BOTTOM, TANK_W + 1, WHITE);
  display.drawFastVLine(TANK_RIGHT, TANK_Y, TANK_H, WHITE);
}

void drawScale() {
  const int marks[] = {0, 4, 8, 12};
  display.setTextSize(1);
  display.setTextColor(WHITE);

  for (int i = 0; i < 4; i++) {
    int value = marks[i];
    int y = TANK_BOTTOM - (int)((value / TANK_CAPACITY_M3) * TANK_H);
    display.drawFastHLine(SCALE_X, y, SCALE_TICK_W, WHITE);

    int textY = y - 3;
    if (textY < 0) textY = 0;
    if (textY > SCREEN_H - 8) textY = SCREEN_H - 8;

    display.setCursor(SCALE_LABEL_X, textY);
    display.print(value);
  }
}

// Геометрия заливки (общая для воды и инверсии цифр). fillH==0 → пусто.
bool waterFillGeom(float volume_m3, int &waterL, int &waterR, int &waterBottom, int &topY, int &fillH) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;

  const int gap = 1;
  waterL = TANK_X + 1 + gap;
  waterR = TANK_RIGHT - 1 - gap;
  waterBottom = TANK_BOTTOM - 1 - gap;
  const int waterW = waterR - waterL + 1;
  if (waterW <= 0) {
    fillH = 0;
    return false;
  }

  const int innerH = waterBottom - (TANK_Y + gap);
  if (innerH <= 0) {
    fillH = 0;
    return false;
  }

  fillH = (int)((volume_m3 / TANK_CAPACITY_M3) * innerH + 0.5f);
  if (fillH <= 0) {
    fillH = 0;
    return false;
  }
  if (fillH > innerH) fillH = innerH;

  topY = waterBottom - fillH + 1;
  if (topY < TANK_Y + gap) topY = TANK_Y + gap;
  return true;
}

// Заливка с зазором 1 px; волна туда-обратно по зеркалу за 5 с
void drawWater(float volume_m3) {
  int waterL, waterR, waterBottom, topY, fillH;
  if (!waterFillGeom(volume_m3, waterL, waterR, waterBottom, topY, fillH)) return;

  const int gap = 1;
  const int waterW = waterR - waterL + 1;

  display.fillRect(waterL, topY, waterW, waterBottom - topY + 1, WHITE);

  if (fillH >= 2) {
    const unsigned long halfMs = 2500UL;
    const unsigned long cycle = millis() % (halfMs * 2UL);
    const int path = waterW - 1;
    int crest;

    if (cycle < halfMs) {
      crest = (int)((cycle * (unsigned long)path) / halfMs);
    } else {
      crest = path - (int)(((cycle - halfMs) * (unsigned long)path) / halfMs);
    }

    for (int x = waterL; x <= waterR; x++) {
      int d = abs((x - waterL) - crest);
      if (d > 7) continue;
      int bump = 1 + (7 - d) / 3;
      for (int b = 1; b <= bump; b++) {
        int y = topY - b;
        if (y >= TANK_Y + gap) display.drawPixel(x, y, WHITE);
      }
    }
  }
}

// XX.YY в чёрном «окне» по центру.
// Заливка рисуется целиком раньше → когда зеркало выше окна, вода уже сомкнута
// вокруг/над ним одним куском; окно только вырезает место под цифры.
void drawVolumeValue(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;

  int m3 = (int)volume_m3;
  int centi = (int)((volume_m3 - (float)m3) * 100.0f + 0.5f);
  if (centi >= 100) {
    m3++;
    centi = 0;
  }

  char buf[8];
  if (!sensorHasReading) {
    // Диагностика: no.rx = ни одного байта; bd.fr = байты есть, кадр не собран
    if (sensorBytesRx == 0) {
      snprintf(buf, sizeof(buf), "no.rx");
    } else {
      snprintf(buf, sizeof(buf), "bd.fr");
    }
  } else {
    snprintf(buf, sizeof(buf), "%d.%02d", m3, centi);
  }

  const int tw = (int)strlen(buf) * 12;
  const int th = 16;
  const int padX = 3;
  const int padY = 2;
  const int tx = TANK_X + (TANK_W - tw) / 2;
  const int ty = TANK_Y + (TANK_H - th) / 2;

  // Чёрное окно (вырез в заливке) + белые цифры
  display.fillRect(tx - padX, ty - padY, tw + padX * 2, th + padY * 2, BLACK);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(tx, ty);
  display.print(buf);
}

void drawSensorDebug() {
  // Сверху: либо «NN cm», либо сырые байты HEX (VT «fg» = бинарник, смотрим код)
  char dbg[22];
  if (sensorHasReading) {
    snprintf(dbg, sizeof(dbg), "%d cm", (int)(lastDistance_cm + 0.5f));
  } else {
    snprintf(dbg, sizeof(dbg), "%02X%02X%02X%02X",
             lastRx[0], lastRx[1], lastRx[2], lastRx[3]);
  }
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int tw = (int)strlen(dbg) * 6;
  display.setCursor((SCREEN_W - tw) / 2, 0);
  display.print(dbg);
}

void drawInterface(float volume_m3) {
  display.clearDisplay();

  drawWater(volume_m3);   // динамика
  drawTankFrame();        // статика поверх зазоров
  drawScale();
  drawVolumeValue(volume_m3);
  drawSensorDebug();

  display.display();
}

// =============================================================================
// Датчик
// Датчик на горле смотрит вниз:
//   distance = 180 см → пусто (0 м³)
//   distance = 0 см   → полно (12 м³)
//   level_m = 1.8 - distance_m
// =============================================================================
#if defined(SENSOR_MODE_UART)
static void pushLastRx(uint8_t b) {
  lastRx[0] = lastRx[1];
  lastRx[1] = lastRx[2];
  lastRx[2] = lastRx[3];
  lastRx[3] = b;
}

// Кадр Proteus ET (факт): 00 HH LL YY, где (HH<<8|LL) = cm * 8, YY — хвост.
// Пример: 3 см → 00 00 18 F0 (0x18=24=3*8); 180 см → 00 05 A0 ?? (1440=180*8).
bool tryParseProteusEtQuirk(uint16_t &distance_raw) {
  if (lastRx[0] != 0x00) return false;
  uint16_t scaled = ((uint16_t)lastRx[1] << 8) | lastRx[2];
  if (scaled == 0) return false;
  if ((scaled & 0x07) != 0) return false;  // кратно 8
  uint16_t cm = scaled >> 3;
  if (cm > 400) return false;
  distance_raw = cm;
  return true;
}

bool readUartDistance(uint16_t &distance_raw) {
  static uint8_t state = 0;
  static uint8_t H = 0, L = 0;
  bool got = false;

  while (SENSOR_PORT.available()) {
    uint8_t b = (uint8_t)SENSOR_PORT.read();
    sensorBytesRx++;
    pushLastRx(b);

    // --- стандарт JSN / документация ET: FF H L CHK ---
    if (b == 0xFF) {
      state = 1;
      continue;
    }

    switch (state) {
      case 0:
        break;
      case 1:
        H = b;
        state = 2;
        break;
      case 2:
        L = b;
        state = 3;
        break;
      case 3: {
        state = 0;
        uint16_t dist = ((uint16_t)H << 8) | L;
        uint8_t chkEt = (uint8_t)(0xFF + H + L);
        uint8_t chkSum = (uint8_t)(H + L);
        bool ok = (b == chkEt) || (b == chkSum);
#if !SENSOR_UART_STRICT_CHECKSUM
        // без жёсткой суммы — но только если расстояние правдоподобно (не 0x18F0 → «180»)
        ok = ok || (dist <= 400);
#endif
        if (ok && dist <= 400) {
          distance_raw = dist;
          sensorFramesOk++;
          got = true;
        }
        break;
      }
    }

    // --- обходной разбор кадра Proteus (00 00 XX YY, XX=cm*8) ---
    if (!got && tryParseProteusEtQuirk(distance_raw)) {
      sensorFramesOk++;
      got = true;
    }
  }
  return got;
}

void requestUartMeasure() {
  // JSN: 0x55. Модель ET в MANUAL тоже ждёт байт на RX — шлём 0x55.
  SENSOR_PORT.write((uint8_t)0x55);
}

void flushSensorSerial() {
  while (SENSOR_PORT.available()) {
    (void)SENSOR_PORT.read();
    // не считаем в sensorBytesRx — это сброс мусора до старта
  }
}
#endif

#if defined(SENSOR_MODE_TRIG_ECHO)
bool readTrigEchoDistanceCm(float &distance_cm) {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return false;
  distance_cm = duration * 0.0343f / 2.0f;
  return true;
}
#endif

void applyDistanceCm(float distance_cm) {
  // Мусор (например 0x18F0 = 6384) раньшеclamp'ился в 180 → ложные «180 cm» и «0.00».
  // Такие кадры просто отбрасываем, оставляем прошлое валидное значение.
  if (distance_cm < SENSOR_MIN_CM || distance_cm > SENSOR_MAX_CM) {
    return;
  }

  lastDistance_cm = distance_cm;
  currentLevel_m = levelFromDistance_m(distance_cm / 100.0f);
  sensorHasReading = true;
}

void setupSensor() {
#if defined(SENSOR_MODE_TRIG_ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
#elif defined(SENSOR_MODE_UART)
  SENSOR_PORT.begin(9600);
  delay(100);
  flushSensorSerial();
  requestUartMeasure();  // на случай MODE=MANUAL
#else
#error "Выбери SENSOR_MODE_TRIG_ECHO или SENSOR_MODE_UART"
#endif
}

// Читает все доступные кадры, оставляет последнее валидное расстояние
bool updateLevelFromSensor() {
  bool got = false;

#if defined(SENSOR_MODE_UART)
  uint16_t raw = 0;
  while (readUartDistance(raw)) {
#if SENSOR_DIST_IS_MM
    applyDistanceCm(raw / 10.0f);   // мм → см
#else
    applyDistanceCm((float)raw);    // Proteus ET: уже см
#endif
    got = true;
  }
#elif defined(SENSOR_MODE_TRIG_ECHO)
  float distance_cm = 0;
  if (readTrigEchoDistanceCm(distance_cm)) {
    applyDistanceCm(distance_cm);
    got = true;
  }
#endif

  return got;
}

void setup() {
  setupSensor();

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  drawInterface(volumeFromLevel_m3(currentLevel_m));
}

void loop() {
#if defined(SENSOR_MODE_UART)
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll >= SENSOR_POLL_MS) {
    lastPoll = now;
    requestUartMeasure();  // AUTO: лишний байт обычно ок; MANUAL: обязателен
  }
#endif

  updateLevelFromSensor();
  drawInterface(volumeFromLevel_m3(currentLevel_m));
  delay(50);
}
