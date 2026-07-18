# HANDOFF — Ultrasonic level gauge

Обновлено: 2026-07-18

## Статус
Скетч на UART. Proteus всё ещё падает: модель не в `MODELS` + на схеме, похоже, остался TRIG/ECHO (пины TR/ECHO).

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
- [x] UI ёмкости + шкала 0/4/8/12
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

## Блокер Proteus #3 — чёрный OLED
Симптом: симуляция идёт, дисплей чёрный; в примере тот же OLED оживал.

Чеклист:
1. **Пересобери hex** (Sketch → Export compiled binary) и в Proteus на Arduino:  
   Edit Properties → **Program File** → новый  
   `sketch_jul17a.ino.eightanaloginputs.hex`
2. В схеме **RES OLED → VCC** (не D4). В скетче сейчас `OLED_RESET = -1` как в примерах.
3. SDA→A4, SCL→A5, общий GND, VCC дисплея.
4. После старта должен мелькнуть **белый экран ~0.5 с**, потом UI.  
   Если белого нет — hex/модель/проводка, не отрисовка ёмкости.
5. Если пример на **U8g2/U8glib**, а не Adafruit — напиши, перепишем UI на ту же либу.

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
