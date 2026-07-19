# SDD — Ultrasonic level gauge

## Цель
Уровнемер жидкости: Arduino Nano 3 + JSN-SR04T + OLED 0.96" 128×64 I2C.  
На дисплее — открытая ёмкость со шкалой объёма; объём считается по высоте столба воды.

## Железо
| Узел | Модель / примечание |
|------|---------------------|
| МК | Arduino Nano 3 (ATmega328) |
| Датчик | JSN-SR04T / в Proteus: **Electronics Tree HCSR04 UART** |
| Дисплей | OLED 0.96" 128×64 I2C (SSD1306) — в Proteus-примере адрес **`0x3D`** |
| Среда проверки (сейчас) | Proteus |

## Пины (Nano)
| Сигнал | Пин | Примечание |
|--------|-----|------------|
| OLED SDA | A4 | I2C |
| OLED SCL | A5 | I2C |
| OLED RES | **D4** | как в рабочем примере Proteus: `Adafruit_SSD1306 display(4)` |
| Датчик UART TX→МК | **D0 / PD0 / RX0** | Sensor TX → Nano RX (Hardware Serial) |
| Датчик UART RX←МК | **D1 / PD1 / TX0** | Sensor RX → Nano TX (для MANUAL) |
| SoftSerial | не для Proteus | в симуляции SoftSerial часто мёртв |
| Датчик TRIG | D9 / PB1 | запас TRIG/ECHO |
| Датчик ECHO | D10 / PB2 | запас TRIG/ECHO |
| Питание | +5V / GND | общий GND |

## Режимы датчика
В скетче `#define`:

- **`SENSOR_MODE_UART`** — **активен сейчас** (Proteus HCSR04_UART).
- **`SENSOR_MODE_TRIG_ECHO`** — закомментирован; нужен файл модели `HCSR04.MDF`.

### UART-протокол (Electronics Tree / совместимый кадр)
- 9600 8N1
- Кадр 4 байта: `0xFF | Dist_H | Dist_L | CHK`
- `CHK = (0xFF + Dist_H + Dist_L) & 0xFF`
- `distance = (H << 8) | L`
- В модели Proteus ET расстояние в кадре — **см** (`SENSOR_DIST_IS_MM 0`)
- Реальный JSN-SR04T часто отдаёт **мм** → `SENSOR_DIST_IS_MM 1`
- AUTO: кадры сами; MANUAL: в `loop` раскомментировать `requestUartMeasure()` (байт `0x55`)

### Связь дистанция → уровень → объём
Датчик на горле, смотрит вниз. В Proteus MAX датчика = **180 см** (= `TANK_HEIGHT_M`):
- `180 см` → пусто `0.00` м³; `0 см` → полно `12.00` м³
- `level_m = TANK_HEIGHT_M - (distance_m - SENSOR_MOUNT_OFFSET_M)`
- `volume_m3 = level_m / TANK_HEIGHT_M * TANK_CAPACITY_M3`
- опрос UART каждые `SENSOR_POLL_MS` (200) байтом `0x55`

## Параметры ёмкости (калибровка)
| Параметр | Значение | Где править |
|----------|----------|-------------|
| Полный объём | `12.0` м³ | `TANK_CAPACITY_M3` |
| Высота дно → горло | `1.8` м | `TANK_HEIGHT_M` |
| Смещение датчика | `0.0` м | `SENSOR_MOUNT_OFFSET_M` |

## UI (OLED)
- Ёмкость без верхней крышки, отступы 4 px от краёв экрана.
- **Статика:** рамка ёмкости + шкала 0/4/8/12.
- **Динамика:** заливка с зазором **1 px** от стенок/дна; волна по зеркалу туда-обратно за **5 с** (2.5 с L→R, 2.5 с R→L).
- По центру чёрное окно с белым **`XX.YY`**. Заливка сплошная; когда зеркало выше окна — вода сомкнута вокруг него одним куском.
- OLED SSD1306 — монохром (1 бит).

## Рабочий файл кода
- `sketch_jul17a/sketch_jul17a.ino`
- Копия в `src/main.cpp` — вручную.
- Библиотеки: Adafruit SSD1306 + GFX.

## Процесс
Задача → код → Proteus → результат → `HANDOFF.md` → следующая задача.
