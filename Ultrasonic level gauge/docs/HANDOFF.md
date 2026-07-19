# HANDOFF — Ultrasonic level gauge

Обновлено: 2026-07-18

## Статус
Датчик UART прикручен к UI. Проверка в Proteus: крутить setpoint 0…180 см.

## Маппинг датчика → объём
| Distance (см) | Уровень | Объём |
|---------------|---------|-------|
| 180 (MAX) | 0 м | 0.00 м³ |
| 90 | 0.9 м | 6.00 м³ |
| 0 | 1.8 м | 12.00 м³ |

`level = 1.8 − distance_m`, опрос UART каждые 200 мс (`0x55` + кадр `FF H L CHK`).

## Блокер Proteus #2
1. В `LIBRARY` есть `ElectronicsTree[HCSR04].LIB`, но в  
   `C:\Program Files (x86)\Labcenter Electronics\Proteus 8 Professional\MODELS`  
   **нет** `HCSR04_UART.MDF` и `ULTRASONICUART.dll` (копирование без админа — Access Denied).
2. Ошибка `Pin 'TR'/'ECHO' is not modelled` = на схеме компонент **HC-SR04 (TRIG/ECHO)**, а не UART.  
   Нужен: **Ultra-Sonic Sensor Module with UART** (пины **TX/RX**, `MODFILE=HCSR04_UART`).

### Фикс
1. PowerShell **от администратора** → `docs/install_proteus_uart_model.ps1`
2. Закрыть/открыть Proteus
3. Удалить старый U2 → Pick Devices → UART-вариант → TX→D8, RX→D7

## Сделано
- [x] UI: ёмкость, шкала 0/4/8/12, ватерлиния, `XX.YY` по центру (принято)
- [x] Калибровка `TANK_HEIGHT_M=1.8`, `TANK_CAPACITY_M3=12`
- [x] Активен `SENSOR_MODE_UART` (SoftSerial D8/D7, 9600)
- [x] Парсер кадра `FF H L CHK`
- [x] `levelFromDistance_m()` → объём на OLED
- [x] TRIG/ECHO оставлен в скетче (закомментирован режим)
- [x] Блокер `HCSR04.MDF` — обход сменой на UART-модель

## Соединения Proteus (UART)
| Датчик | Nano |
|--------|------|
| TX | **D8** |
| RX | **D7** |
| VCC | +5V |
| GND | GND |

OLED без изменений: SDA=A4, SCL=A5, RES=D4.

## Проверить сейчас
- [ ] OLED чёрный → см. блокер #3 ниже
- [ ] MODE датчика: AUTO (или MANUAL + `requestUartMeasure()`)
- [ ] Крутишь setpoint → меняется заливка

## Блокер Proteus #3 — чёрный OLED (причина найдена)
Рабочий пример Proteus (`main.ino` из temp VSM):
- `Adafruit_SSD1306 display(OLED_RESET);` при `OLED_RESET 4`
- `display.begin(SSD1306_SWITCHCAPVCC, 0x3D);` ← адрес **0x3D**, не 0x3C
- старая Adafruit (без проверки `bool` у `begin`)

Скетч приведён к этому init. RES снова на **D4**.

Пересобери в Proteus/VSM и проверь splash → UI ёмкости.

## Следующая задача
*(после успешного UART в Proteus)*  
Уточнить единицы (см/мм) на реальном JSN; при необходимости подписи объёма на OLED.

## Открытые решения
- `SENSOR_DIST_IS_MM` — для Proteus ET = 0 (см); для железа часто 1 (мм)
- Точная `TANK_HEIGHT_M` при калибровке

## Рабочие пути
- Код: `sketch_jul17a/sketch_jul17a.ino`
- Спека: `docs/SDD.md`
- Handoff: `docs/HANDOFF.md`
