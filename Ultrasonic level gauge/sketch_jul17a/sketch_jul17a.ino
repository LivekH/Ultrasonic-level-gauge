/*
 * Ultrasonic level gauge — Arduino Nano 3
 * OLED 0.96" 128x64 I2C (SSD1306) + JSN-SR04T
 *
 * OLED init — как в рабочем примере Proteus (ssd1306_128x64_i2c):
 *   Adafruit_SSD1306 display(OLED_RESET);  RES=D4, адрес 0x3D
 *
 * Датчик UART: TX датчика -> D8, RX датчика -> D7
 * Датчик TRIG/ECHO (запас): TRIG=D9, ECHO=D10
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// РЕЖИМ ДАТЧИКА — выбери один вариант
// =============================================================================
// Вариант A: TRIG/ECHO — закомментируй UART ниже и раскомментируй эту строку
// #define SENSOR_MODE_TRIG_ECHO

// Вариант B (сейчас активен): UART — Proteus Electronics Tree HCSR04_UART / JSN-SR04T
#define SENSOR_MODE_UART

#define TRIG_PIN 9
#define ECHO_PIN 10

#define SENSOR_RX 8   // Nano D8  <- TX датчика
#define SENSOR_TX 7   // Nano D7  -> RX датчика
SoftwareSerial sensorSerial(SENSOR_RX, SENSOR_TX);

// Proteus ET UART: см. Реальный JSN часто мм → поставь 1.
#define SENSOR_DIST_IS_MM 0

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
const float TANK_HEIGHT_M    = 1.8f;    // <<< КАЛИБРОВКА: дно → горло, м
const float TANK_AREA_M2     = TANK_CAPACITY_M3 / TANK_HEIGHT_M;
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

float currentLevel_m = TANK_HEIGHT_M * 0.5f;

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

// Только ватерлиния (верх зеркала воды), без заливки всего объёма
void drawWaterLine(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;

  int fillH = (int)((volume_m3 / TANK_CAPACITY_M3) * TANK_H + 0.5f);
  if (fillH < 0) fillH = 0;
  if (fillH > TANK_H) fillH = TANK_H;

  int y = TANK_BOTTOM - fillH;
  if (y < TANK_Y) y = TANK_Y;
  if (y > TANK_BOTTOM) y = TANK_BOTTOM;

  display.drawFastHLine(TANK_X + 1, y, TANK_W - 1, WHITE);
}

// XX.YY — целые м³ и сотые м³ (= YY×10 литров), напр. 8.56 → 8 м³ + 560 л
void drawVolumeValue(float volume_m3) {
  if (volume_m3 < 0.0f) volume_m3 = 0.0f;
  if (volume_m3 > TANK_CAPACITY_M3) volume_m3 = TANK_CAPACITY_M3;

  int m3 = (int)volume_m3;
  int centi = (int)((volume_m3 - (float)m3) * 100.0f + 0.5f); // 0..99
  if (centi >= 100) {
    m3++;
    centi = 0;
  }

  char buf[8];
  snprintf(buf, sizeof(buf), "%d.%02d", m3, centi);

  display.setTextSize(2);   // символ 12×16
  display.setTextColor(WHITE);

  int tw = (int)strlen(buf) * 12;
  int th = 16;
  int tx = TANK_X + (TANK_W - tw) / 2;
  int ty = TANK_Y + (TANK_H - th) / 2;

  // подложка, чтобы ватерлиния не перечёркивала цифры
  display.fillRect(tx - 2, ty - 1, tw + 4, th + 2, BLACK);
  display.setCursor(tx, ty);
  display.print(buf);
}

void drawInterface(float volume_m3) {
  display.clearDisplay();

  drawWaterLine(volume_m3);
  drawTankFrame();
  drawScale();
  drawVolumeValue(volume_m3);

  display.display();
}

// =============================================================================
// Датчик
// =============================================================================
#if defined(SENSOR_MODE_UART)
bool readUartDistance(uint16_t &distance_raw) {
  static uint8_t state = 0;
  static uint8_t H = 0, L = 0;

  while (sensorSerial.available()) {
    uint8_t b = (uint8_t)sensorSerial.read();
    switch (state) {
      case 0: if (b == 0xFF) state = 1; break;
      case 1: H = b; state = 2; break;
      case 2: L = b; state = 3; break;
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

void requestUartMeasure() {
  sensorSerial.write((uint8_t)0x55);
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

void setupSensor() {
#if defined(SENSOR_MODE_TRIG_ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
#elif defined(SENSOR_MODE_UART)
  sensorSerial.begin(9600);
#else
#error "Выбери SENSOR_MODE_TRIG_ECHO или SENSOR_MODE_UART"
#endif
}

bool updateLevelFromSensor() {
#if defined(SENSOR_MODE_UART)
  uint16_t raw = 0;
  if (!readUartDistance(raw)) return false;
#if SENSOR_DIST_IS_MM
  currentLevel_m = levelFromDistance_m(raw / 1000.0f);
#else
  currentLevel_m = levelFromDistance_m(raw / 100.0f);
#endif
  return true;
#elif defined(SENSOR_MODE_TRIG_ECHO)
  float distance_cm = 0;
  if (!readTrigEchoDistanceCm(distance_cm)) return false;
  currentLevel_m = levelFromDistance_m(distance_cm / 100.0f);
  return true;
#endif
}

void setup() {
  // как в примере Proteus — Serial не обязателен, но begin OLED идентичный
  setupSensor();

  // ВАЖНО для Proteus: адрес 0x3D, RES=D4, старый конструктор
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  // begin() кладёт в буфер splash Adafruit — сразу чистим, без display() со логотипом
  display.clearDisplay();
  drawInterface(volumeFromLevel_m3(currentLevel_m));
}

void loop() {
#if defined(SENSOR_MODE_UART)
  // MANUAL mode: раскомментируй
  // requestUartMeasure();
#endif

  updateLevelFromSensor();
  drawInterface(volumeFromLevel_m3(currentLevel_m));
  delay(200);
}
