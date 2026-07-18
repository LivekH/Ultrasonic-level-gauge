/*
 * Ultrasonic level gauge — Arduino Nano 3
 * OLED 0.96" 128x64 I2C (SSD1306) + JSN-SR04T
 *
 * Пины OLED:  SDA=A4, SCL=A5, RES=D4
 * Пины датчика TRIG/ECHO: TRIG=D9, ECHO=D10
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =============================================================================
// РЕЖИМ ДАТЧИКА — выбери один вариант
// =============================================================================
// Вариант A (сейчас активен): TRIG/ECHO — тест в Proteus
#define SENSOR_MODE_TRIG_ECHO

// Вариант B: UART — раскомментируй строку ниже и закомментируй SENSOR_MODE_TRIG_ECHO выше
// #define SENSOR_MODE_UART

// --- пины TRIG/ECHO (активны при SENSOR_MODE_TRIG_ECHO) ---
#define TRIG_PIN 9
#define ECHO_PIN 10

// --- пины UART (раскомментируй блок при SENSOR_MODE_UART) ---
// #include <SoftwareSerial.h>
// #define SENSOR_RX 8   // Nano D8  <- TX датчика
// #define SENSOR_TX 7   // Nano D7  -> RX датчика
// SoftwareSerial sensorSerial(SENSOR_RX, SENSOR_TX);

// =============================================================================
// OLED
// =============================================================================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    4          // RES дисплея -> D4
#define OLED_ADDR     0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =============================================================================
// Ёмкость / UI
// =============================================================================
const float TANK_CAPACITY_M3 = 12.0f;

// Геометрия «ёмкости» на экране (открытый верх)
const int TANK_X      = 18;
const int TANK_Y      = 6;     // верх стенок (линии сверху нет)
const int TANK_W      = 52;
const int TANK_H      = 50;
const int TANK_BOTTOM = TANK_Y + TANK_H;   // 56
const int TANK_RIGHT  = TANK_X + TANK_W;   // 70

// Шкала справа от ёмкости
const int SCALE_X     = TANK_RIGHT + 4;    // начало засечек
const int SCALE_LABEL_X = SCALE_X + 6;

// Временный уровень для проверки UI в Proteus (м³). Потом заменим данными с датчика.
float demoVolume_m3 = 6.0f;

// -----------------------------------------------------------------------------
void drawTankFrame() {
  // Левая стенка, дно, правая стенка — без верхней крышки
  display.drawFastVLine(TANK_X, TANK_Y, TANK_H, SSD1306_WHITE);
  display.drawFastHLine(TANK_X, TANK_BOTTOM, TANK_W + 1, SSD1306_WHITE);
  display.drawFastVLine(TANK_RIGHT, TANK_Y, TANK_H, SSD1306_WHITE);
}

void drawScale() {
  // Деления: 0, 4, 8, 12 м³ (0 — снизу, 12 — сверху)
  const int marks[] = {0, 4, 8, 12};
  const int n = 4;

  for (int i = 0; i < n; i++) {
    int value = marks[i];
    // y: 12 -> верх ёмкости, 0 -> дно
    int y = TANK_BOTTOM - (int)((value / TANK_CAPACITY_M3) * TANK_H);

    display.drawFastHLine(SCALE_X, y, 4, SSD1306_WHITE);

    // Подпись; для верхней чуть ниже, чтобы не обрезать
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

  // Вода внутри стенок (отступ 1 px), растёт снизу вверх
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

// -----------------------------------------------------------------------------
void setupSensor() {
#if defined(SENSOR_MODE_TRIG_ECHO)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

#elif defined(SENSOR_MODE_UART)
  // sensorSerial.begin(9600);   // типичная скорость JSN-SR04T в UART
#else
#error "Выбери SENSOR_MODE_TRIG_ECHO или SENSOR_MODE_UART"
#endif
}

void setup() {
  // Serial.begin(9600);  // при необходимости отладки

  setupSensor();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // Если дисплей не ответил — зацикливаемся (в Proteus проверь A4/A5/RES/адрес)
    for (;;) { /* halt */ }
  }

  display.clearDisplay();
  display.display();

  drawInterface(demoVolume_m3);
}

void loop() {
  // Пока только UI. Следующая задача — чтение датчика.
  drawInterface(demoVolume_m3);
  delay(500);
}
