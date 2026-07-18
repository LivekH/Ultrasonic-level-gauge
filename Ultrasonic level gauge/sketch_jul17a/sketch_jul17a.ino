/*
 * Ultrasonic level gauge — Arduino Nano 3
 * OLED 0.96" 128x64 I2C (SSD1306) + JSN-SR04T
 *
 * Пины OLED:  SDA=A4, SCL=A5, RES=D4
 * Датчик UART (сейчас): TX датчика -> D8 (RX), RX датчика -> D7 (TX)
 * Датчик TRIG/ECHO (запас): TRIG=D9, ECHO=D10
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

// =============================================================================
// РЕЖИМ ДАТЧИКА — выбери один вариант
// =============================================================================
// Вариант A: TRIG/ECHO — закомментируй UART ниже и раскомментируй эту строку
// #define SENSOR_MODE_TRIG_ECHO

// Вариант B (сейчас активен): UART — Proteus Electronics Tree HCSR04_UART / JSN-SR04T
#define SENSOR_MODE_UART

// --- пины TRIG/ECHO (раскомментируй логику при SENSOR_MODE_TRIG_ECHO) ---
#define TRIG_PIN 9
#define ECHO_PIN 10

// --- пины UART ---
#define SENSOR_RX 8   // Nano D8  <- TX датчика
#define SENSOR_TX 7   // Nano D7  -> RX датчика (нужен в MANUAL mode)
SoftwareSerial sensorSerial(SENSOR_RX, SENSOR_TX);

// Proteus Electronics Tree UART: в кадре расстояние в САНТИМЕТРАХ.
// Реальный JSN-SR04T UART часто отдаёт МИЛЛИМЕТРЫ — тогда поставь 1.
#define SENSOR_DIST_IS_MM 0

// =============================================================================
// OLED (Proteus)
// =============================================================================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// В рабочих примерах Proteus RES OLED часто сидит на VCC, а в коде reset = -1.
// Если оставить RES на D4 и reset=4 — часть моделей остаётся чёрной.
// Вариант A (как в примерах): RES → VCC, OLED_RESET = -1
// Вариант B: RES → D4, OLED_RESET = 4
#define OLED_RESET    (-1)
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =============================================================================
// ПАРАМЕТРЫ РЕАЛЬНОЙ ЁМКОСТИ (калибровка)
// =============================================================================
// Квадратное сечение → объём пропорционален высоте столба воды.
const float TANK_CAPACITY_M3 = 12.0f;   // полный объём, м³
const float TANK_HEIGHT_M    = 1.8f;    // <<< КАЛИБРОВКА: высота от дна до горла, м

// Площадь сечения (м²)
const float TANK_AREA_M2 = TANK_CAPACITY_M3 / TANK_HEIGHT_M;  // 12 / 1.8 ≈ 6.667 м²

// Датчик смотрит сверху вниз с горла: level = высота_ёмкости - дистанция_до_зеркала
// Если датчик чуть выше горла — добавь смещение сюда (м).
const float SENSOR_MOUNT_OFFSET_M = 0.0f;

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

// Дистанция датчик→вода (м) → уровень воды от дна (м)
float levelFromDistance_m(float distance_m) {
  float level = TANK_HEIGHT_M - (distance_m - SENSOR_MOUNT_OFFSET_M);
  if (level < 0.0f) level = 0.0f;
  if (level > TANK_HEIGHT_M) level = TANK_HEIGHT_M;
  return level;
}

// =============================================================================
// Геометрия «ёмкости» на OLED (открытый верх)
// =============================================================================
const int TANK_X      = 18;
const int TANK_Y      = 6;
const int TANK_W      = 52;
const int TANK_H      = 50;
const int TANK_BOTTOM = TANK_Y + TANK_H;
const int TANK_RIGHT  = TANK_X + TANK_W;

const int SCALE_X       = TANK_RIGHT + 4;
const int SCALE_LABEL_X = SCALE_X + 6;

// Последний успешный уровень (стартуем с половины — пока нет кадра)
float currentLevel_m = TANK_HEIGHT_M * 0.5f;
bool g_oledOk = false;

// -----------------------------------------------------------------------------
void drawTankFrame() {
  display.drawFastVLine(TANK_X, TANK_Y, TANK_H, SSD1306_WHITE);
  display.drawFastHLine(TANK_X, TANK_BOTTOM, TANK_W + 1, SSD1306_WHITE);
  display.drawFastVLine(TANK_RIGHT, TANK_Y, TANK_H, SSD1306_WHITE);
}

void drawScale() {
  const int marks[] = {0, 4, 8, 12};
  const int n = 4;

  for (int i = 0; i < n; i++) {
    int value = marks[i];
    int y = TANK_BOTTOM - (int)((value / TANK_CAPACITY_M3) * TANK_H);

    display.drawFastHLine(SCALE_X, y, 4, SSD1306_WHITE);

    int textY = y - 3;
    if (textY < 0) textY = 0;
    if (textY > SCREEN_HEIGHT - 8) textY = SCREEN_HEIGHT - 8;

    display.setCursor(SCALE_LABEL_X, textY);
    display.print(value);
  }
}

void drawWater(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;

  int fillH = (int)((volume_m3 / TANK_CAPACITY_M3) * TANK_H);
  if (fillH <= 0) return;

  int fillY = TANK_BOTTOM - fillH;
  display.fillRect(TANK_X + 1, fillY, TANK_W - 1, fillH, SSD1306_WHITE);
}

void drawInterface(float volume_m3) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  drawWater(volume_m3);
  drawTankFrame();
  drawScale();

  display.display();
}

// =============================================================================
// Датчик
// =============================================================================
#if defined(SENSOR_MODE_UART)
// Кадр Electronics Tree / JSN-UART: FF | H | L | CHK, CHK = (FF+H+L) & 0xFF
// Возвращает true и пишет distance_raw (см или мм — см. SENSOR_DIST_IS_MM)
bool readUartDistance(uint16_t &distance_raw) {
  static uint8_t state = 0;
  static uint8_t H = 0, L = 0;

  while (sensorSerial.available()) {
    uint8_t b = (uint8_t)sensorSerial.read();

    switch (state) {
      case 0:
        if (b == 0xFF) state = 1;
        break;
      case 1:
        H = b;
        state = 2;
        break;
      case 2:
        L = b;
        state = 3;
        break;
      case 3:
        state = 0;
        if (((0xFF + H + L) & 0xFF) == b) {
          distance_raw = ((uint16_t)H << 8) | L;
          return true;
        }
        break;
    }
  }
  return false;
}

// MANUAL mode Proteus: раскомментируй вызов в loop(), если модель не шлёт сама (AUTO)
void requestUartMeasure() {
  sensorSerial.write((uint8_t)0x55);  // типичный триггер JSN-SR04T; у ET уточни MODE
}
#endif

#if defined(SENSOR_MODE_TRIG_ECHO)
bool readTrigEchoDistanceCm(float &distance_cm) {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // таймаут ~30 мс
  if (duration == 0) return false;

  distance_cm = duration * 0.0343f / 2.0f;
  return true;
}
#endif

void setupSensor() {
#if defined(SENSOR_MODE_TRIG_ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

#elif defined(SENSOR_MODE_UART)
  sensorSerial.begin(9600);  // 8N1, как в Electronics Tree / JSN-SR04T UART
#else
#error "Выбери SENSOR_MODE_TRIG_ECHO или SENSOR_MODE_UART"
#endif
}

// true — уровень обновлён из датчика
bool updateLevelFromSensor() {
#if defined(SENSOR_MODE_UART)
  uint16_t raw = 0;
  if (!readUartDistance(raw)) return false;

  float distance_m;
#if SENSOR_DIST_IS_MM
  distance_m = raw / 1000.0f;
#else
  distance_m = raw / 100.0f;   // Proteus ET: см
#endif
  currentLevel_m = levelFromDistance_m(distance_m);
  return true;

#elif defined(SENSOR_MODE_TRIG_ECHO)
  float distance_cm = 0;
  if (!readTrigEchoDistanceCm(distance_cm)) return false;
  currentLevel_m = levelFromDistance_m(distance_cm / 100.0f);
  return true;
#endif
}

bool initOled() {
  Wire.begin();
  delay(50);

  // Сначала 0x3C, затем 0x3D — у модулей бывает разный адрес
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return true;
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) return true;

  // Некоторые модели Proteus лучше поднимаются так:
  if (display.begin(SSD1306_EXTERNALVCC, OLED_ADDR)) return true;
  return false;
}

void setup() {
  setupSensor();

  // НЕ зависаем навечно: в Proteus зависание = вечный чёрный экран без подсказок
  g_oledOk = initOled();
  if (!g_oledOk) return;

  // Тест: белый экран ~0.5 с — если и он чёрный, проблема wiring/hex/модели, не UI
  display.clearDisplay();
  display.fillScreen(SSD1306_WHITE);
  display.display();
  delay(500);

  drawInterface(volumeFromLevel_m3(currentLevel_m));
}

void loop() {
  if (!g_oledOk) {
    g_oledOk = initOled();
    if (!g_oledOk) {
      delay(500);
      return;
    }
  }

#if defined(SENSOR_MODE_UART)
  // Если в Proteus у датчика MODE = MANUAL — раскомментируй:
  // requestUartMeasure();
#endif

  updateLevelFromSensor();
  drawInterface(volumeFromLevel_m3(currentLevel_m));
  delay(200);
}
