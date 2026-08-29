/*
 * Ultrasonic level gauge — Arduino Nano 3
 * OLED 0.96" 128x64 I2C (SSD1306) + ультразвук TRIG/ECHO
 *
 * Калибровка ёмкости/датчика: блок «КАЛИБРОВКА» ниже (все метки <<<).
 *
 * OLED: SDA=A4, SCL=A5; 4-пин → OLED_RESET=-1, адрес обычно 0x3C
 * Датчик TRIG/ECHO: TRIG=D9, ECHO=D10
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// РЕЖИМ ДАТЧИКА
// =============================================================================
// Сейчас TRIG/ECHO — на железе UART так и не дал байт (no.rx).
#define SENSOR_MODE_TRIG_ECHO
// #define SENSOR_MODE_UART

#define TRIG_PIN 9
#define ECHO_PIN 10

// --- UART (если снова включишь SENSOR_MODE_UART) ---
// 0 = SoftSerial D6/D7 при USB; 1 = HW Serial D0/D1 (конфликт с USB!)
#define SENSOR_UART_HW 0

#if defined(SENSOR_MODE_UART) && (SENSOR_UART_HW == 0)
#include <SoftwareSerial.h>
#define SENSOR_RX 6
#define SENSOR_TX 7
SoftwareSerial sensorSerial(SENSOR_RX, SENSOR_TX);
#define SENSOR_PORT sensorSerial
#elif defined(SENSOR_MODE_UART)
#define SENSOR_PORT Serial
#endif

#define SENSOR_DIST_IS_MM 1
#define SENSOR_UART_STRICT_CHECKSUM 0
#define SENSOR_UART_MANUAL_TRIGGER 1
const unsigned long SENSOR_POLL_MS = 300UL;
#define PROTEUS_ET_QUIRK 0
#define SENSOR_DEBUG_OSD 1

// =============================================================================
// OLED — реальное железо 0.96"
// =============================================================================
// 4 пина (VCC GND SCL SDA): OLED_RESET = -1, RES не подключать
// 7 пинов с RES: OLED_RESET = 4 и RES -> D4
#define OLED_RESET (-1)

// У китайских 0.96" почти всегда 0x3C (в Proteus было 0x3D)
#define OLED_ADDR  0x3C

Adafruit_SSD1306 display(OLED_RESET);

// Старая: WHITE/BLACK. Новая: SSD1306_WHITE.
#ifndef WHITE
#define WHITE SSD1306_WHITE
#endif
#ifndef BLACK
#define BLACK SSD1306_BLACK
#endif

// =============================================================================
// КАЛИБРОВКА ЁМКОСТИ И ДАТЧИКА — крути ТОЛЬКО этот блок
// =============================================================================
// Картина для этой ёмкости (датчик на горловине смотрит вниз):
//
//   пусто ──► датчик → дно     ≈ 185 см  →  на OLED сверху: 0 cm,   объём 0.00
//   полно ──► мёртвая зона     ≈  20 см  →  на OLED сверху: 165 cm, объём 12.00
//
// Верхняя надпись = высота ВОДЫ (0…165), не дистанция датчика.
//
// Калибровка рулеткой:
//   DIST_FULL_CM  — зазор датчик↔зеркало при полной (~20 см)
//   DIST_EMPTY_CM — зазор датчик↔дно при пустой ≈ 165 + DIST_FULL (= 185)
//   TANK_HEIGHT_M / TANK_CAPACITY_M3 — паспорт ёмкости
//   DIST_OFFSET_CM / CALIB_TOL_CM — мелкий сдвиг и допуск краёв
// =============================================================================

const float TANK_CAPACITY_M3 = 12.0f;   // <<< полный объём, м³
const float TANK_HEIGHT_M    = 1.65f;   // <<< высота дно→горло, м

const float DIST_EMPTY_CM = 185.0f;     // <<< пусто: рулетка датчик→дно, см
const float DIST_FULL_CM  = 20.0f;      // <<< полно: рулетка датчик→зеркало, см

const float DIST_OFFSET_CM = 0.0f;      // <<< сдвиг, см (+ = больше воды)
const float CALIB_TOL_CM   = 1.0f;      // <<< допуск полно/пусто, см

float volumeFromLevel_m3(float level_m) {
  if (level_m < 0.0f) level_m = 0.0f;
  if (level_m > TANK_HEIGHT_M) level_m = TANK_HEIGHT_M;
  return level_m / TANK_HEIGHT_M * TANK_CAPACITY_M3;
}

float levelFromVolume_m(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;
  return volume_m3 / TANK_CAPACITY_M3 * TANK_HEIGHT_M;
}

// Дистанция датчик→зеркало → уровень воды (м) по калибровке пусто/полно
float levelFromDistance_cm(float distance_cm) {
  float d = distance_cm + DIST_OFFSET_CM;
  float span = DIST_EMPTY_CM - DIST_FULL_CM;
  if (span < 1.0f) span = 1.0f;  // защита

  // Допуск: шум у краёв не даёт 11.99 вместо 12.00
  if (d <= DIST_FULL_CM + CALIB_TOL_CM) return TANK_HEIGHT_M;
  if (d >= DIST_EMPTY_CM - CALIB_TOL_CM) return 0.0f;

  float frac = (DIST_EMPTY_CM - d) / span;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  return frac * TANK_HEIGHT_M;
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

float currentLevel_m = 0.0f;
float lastDistance_cm = DIST_EMPTY_CM;
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
  // Подписи шкалы под TANK_CAPACITY_M3=12. Если ёмкость другая —
  // поменяй marks[] (например 0,5,10,15 при 15 м³).
  const int marks[] = {0, 4, 8, 12};  // <<< деления шкалы, м³
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

  int centiTotal = (int)(volume_m3 * 100.0f + 0.5f);
  const int maxCenti = (int)(TANK_CAPACITY_M3 * 100.0f + 0.5f);
  if (centiTotal < 0) centiTotal = 0;
  if (centiTotal > maxCenti) centiTotal = maxCenti;
  int m3 = centiTotal / 100;
  int centi = centiTotal % 100;

  char buf[12];
  if (!sensorHasReading) {
#if defined(SENSOR_MODE_UART)
    if (sensorBytesRx == 0) {
      snprintf(buf, sizeof(buf), "no.rx");
    } else {
      snprintf(buf, sizeof(buf), "%02X%02X%02X%02X",
               lastRx[0], lastRx[1], lastRx[2], lastRx[3]);
    }
#else
    snprintf(buf, sizeof(buf), "no.ech");
#endif
  } else {
    snprintf(buf, sizeof(buf), "%d.%02d", m3, centi);
  }

  // textSize 1 для hex (8 символов), 2 для объёма
  const int textSize = sensorHasReading ? 2 : 1;
  const int charW = textSize * 6;
  const int tw = (int)strlen(buf) * charW;
  const int th = textSize * 8;
  const int padX = 3;
  const int padY = 2;
  const int tx = TANK_X + (TANK_W - tw) / 2;
  const int ty = TANK_Y + (TANK_H - th) / 2;

  display.fillRect(tx - padX, ty - padY, tw + padX * 2, th + padY * 2, BLACK);
  display.setTextSize(textSize);
  display.setTextColor(WHITE);
  display.setCursor(tx, ty);
  display.print(buf);
}

// Верхняя надпись: высота столба воды 0…TANK_HEIGHT (см), не «сырая» дистанция.
// Чёрное окно — чтобы цифры читались, когда вода заливает верх ёмкости.
void drawLevelCmLabel() {
#if SENSOR_DEBUG_OSD
  char dbg[22];
  if (sensorHasReading) {
    int cm = (int)(currentLevel_m * 100.0f + 0.5f);
    const int maxCm = (int)(TANK_HEIGHT_M * 100.0f + 0.5f);
    if (cm < 0) cm = 0;
    if (cm > maxCm) cm = maxCm;
    snprintf(dbg, sizeof(dbg), "%d cm", cm);
  } else {
    snprintf(dbg, sizeof(dbg), "%02X%02X%02X%02X",
             lastRx[0], lastRx[1], lastRx[2], lastRx[3]);
  }

  display.setTextSize(1);
  const int tw = (int)strlen(dbg) * 6;
  const int th = 8;
  const int padX = 3;
  const int padY = 1;
  const int tx = TANK_X + (TANK_W - tw) / 2;
  const int ty = TANK_Y + 1;

  display.fillRect(tx - padX, ty - padY, tw + padX * 2, th + padY * 2, BLACK);
  display.setTextColor(WHITE);
  display.setCursor(tx, ty);
  display.print(dbg);
#endif
}

void drawInterface(float volume_m3) {
  display.clearDisplay();

  drawWater(volume_m3);
  drawTankFrame();
  drawScale();
  drawVolumeValue(volume_m3);
  drawLevelCmLabel();

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

#if PROTEUS_ET_QUIRK
bool tryParseProteusEtQuirk(uint16_t &distance_raw) {
  if (lastRx[0] != 0x00 || lastRx[3] != 0xF0) return false;
  uint16_t s = ((uint16_t)lastRx[1] << 8) | lastRx[2];
  if (s == 0) return false;
  uint16_t cm = 0;
  if (s > 255 && (s & 0x07) == 0) cm = s >> 3;
  else if (lastRx[1] == 0 && (lastRx[2] & 0x07) == 0) cm = (uint16_t)lastRx[2] >> 3;
  else if (s <= 400) cm = s;
  else return false;
  if (cm < 1 || cm > 400) return false;
  distance_raw = cm;
  return true;
}
#endif

bool readUartDistance(uint16_t &distance_raw) {
  static uint8_t state = 0;
  static uint8_t H = 0, L = 0;
  bool got = false;

  while (SENSOR_PORT.available()) {
    uint8_t b = (uint8_t)SENSOR_PORT.read();
    sensorBytesRx++;
    pushLastRx(b);

    // В Serial Monitor (USB) сыпем байты — Tools → Serial Monitor 9600
    Serial.print(b >> 4, HEX);
    Serial.print(b & 0x0F, HEX);
    Serial.print(' ');

    // Стандарт JSN-SR04T: FF H L CHK
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
        uint8_t chk = (uint8_t)(0xFF + H + L);
        uint8_t chkAlt = (uint8_t)(H + L);
        bool ok = (b == chk) || (b == chkAlt);
#if !SENSOR_UART_STRICT_CHECKSUM
        ok = true;  // временно: любой хвост после FF H L
#endif
        // мм: 20…5000; если датчик шлёт см — тоже поймаем (20…400)
        if (ok && dist >= 2 && dist <= 5000) {
          distance_raw = dist;
          sensorFramesOk++;
          got = true;
          Serial.print(F(" => "));
          Serial.println(dist);
        }
        break;
      }
    }

#if PROTEUS_ET_QUIRK
    if (!got && tryParseProteusEtQuirk(distance_raw)) {
      sensorFramesOk++;
      got = true;
    }
#endif
  }
  return got;
}

void requestUartMeasure() {
  // Мигаем LED на Nano (D13) — видно, что опрос идёт
  digitalWrite(LED_BUILTIN, HIGH);
#if defined(SENSOR_MODE_UART) && (SENSOR_UART_HW == 0)
  sensorSerial.listen();
#endif
  SENSOR_PORT.write((uint8_t)0x55);
  digitalWrite(LED_BUILTIN, LOW);
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
  // Физический диапазон датчика (~2…400 см). Калибровка пусто/полно
  // только пересчитывает объём, не должна отбрасывать замеры со стола.
  if (distance_cm < 2.0f || distance_cm > 400.0f) {
    return;
  }

  lastDistance_cm = distance_cm;
  currentLevel_m = levelFromDistance_cm(distance_cm);
  sensorHasReading = true;
}

void setupSensor() {
#if defined(SENSOR_MODE_TRIG_ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
#elif defined(SENSOR_MODE_UART)
  pinMode(LED_BUILTIN, OUTPUT);
  SENSOR_PORT.begin(9600);
  delay(100);
  flushSensorSerial();
#if SENSOR_UART_MANUAL_TRIGGER
  requestUartMeasure();
#endif
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
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  setupSensor();

  Wire.begin();
  delay(200);

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.print(F("OLED addr 0x"));
  Serial.println(OLED_ADDR, HEX);
#if defined(SENSOR_MODE_TRIG_ECHO)
  Serial.println(F("Sensor: TRIG/ECHO D9/D10"));
#else
  Serial.println(F("Sensor: UART SoftSerial D6/D7"));
#endif

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 8);
#if defined(SENSOR_MODE_TRIG_ECHO)
  display.println(F("TRIG"));
  display.println(F("D9/D10"));
#else
  display.println(F("UART"));
  display.println(F("D6/D7"));
#endif
  display.display();
  delay(1500);

  drawInterface(volumeFromLevel_m3(currentLevel_m));
}

void loop() {
#if defined(SENSOR_MODE_UART) && SENSOR_UART_MANUAL_TRIGGER
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (now - lastPoll >= SENSOR_POLL_MS) {
    lastPoll = now;
    flushSensorSerial();
    requestUartMeasure();
    delay(80);
  }
#endif

  bool got = updateLevelFromSensor();
  if (got) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(30);
    digitalWrite(LED_BUILTIN, LOW);
  }

  drawInterface(volumeFromLevel_m3(currentLevel_m));
  delay(100);
}