# HANDOFF — Ultrasonic level gauge

Обновлено: 2026-07-19

## Статус
**ПАУЗА — ожидание железа** (докупка OLED 0.96" + JSN-SR04T, сборка тестового стенда).

UI и прошивка под реальный UART готовы. Proteus UART ET отложена (модель глючит).  
Когда железо будет — продолжаем отладку с этого handoff.

## Что значат свойства датчика в Proteus
| Поле | У тебя | Смысл |
|------|--------|--------|
| `MODFILE=HCSR04_UART` | да | UART-модель (не TRIG/ECHO) |
| `MODE=0` | AUTO | кадр **только при изменении** setpoint ▲/▼ |
| `SETPOINT=163.0` | см | текущая «дистанция» |
| `MAX=400` | см | потолок модели |
| `FORMAT=3.1` | | отображение с 1 знаком после запятой |
| `MODDLL=SETPOINT.DLL` | | крутилка значения на компоненте |

`HCSR04.MDF` (TRIG/ECHO) в комплекте ET **нет** — только `HCSR04_UART.MDF`.

## Скетч под реальное железо (сейчас)
Файл: `sketch_jul17a/sketch_jul17a.ino`

| Опция | Значение |
|-------|----------|
| `SENSOR_MODE_UART` | вкл |
| `SENSOR_UART_HW` | **0** (SoftSerial D6/D7) |
| `SENSOR_DIST_IS_MM` | **1** (мм → см) |
| `SENSOR_UART_MANUAL_TRIGGER` | **1** (опрос `0x55`) |
| `SENSOR_UART_STRICT_CHECKSUM` | **1** |
| `PROTEUS_ET_QUIRK` | **0** |
| `SENSOR_DEBUG_OSD` | **1** (сверху см / no.rx) |

### Проводка реального JSN-SR04T (UART)
| JSN | Nano |
|-----|------|
| TX | **D6** |
| RX | **D7** |
| VCC | 5V |
| GND | GND |

OLED: SDA=A4, SCL=A5, RES=D4, адрес `0x3D`.

Если модуль в режиме автовыдачи UART (без 0x55) → `SENSOR_UART_MANUAL_TRIGGER 0`.  
Если TRIG/ECHO → `SENSOR_MODE_TRIG_ECHO`, пины D9/D10.

## Маппинг
`distance` от зеркала вниз с горла:  
`180 см` → `0.00` м³; `0 см` → `12.00` м³  
`level = 1.8 − distance_m`

## Сделано (UI)
- [x] Ёмкость, шкала 0/4/8/12, заливка, зазор 1 px, волна, `XX.YY` в окне
- [x] Калибровка `TANK_HEIGHT_M=1.8`, `TANK_CAPACITY_M3=12`
- [x] docs/SDD.md

## Следующее
1. Собрать на столе Nano + OLED + JSN  
2. Залить скетч, проверить кадр `FF H L CHK` (сверху см)  
3. Калибровка `TANK_HEIGHT_M` / `SENSOR_MOUNT_OFFSET_M` на реальной ёмкости  
4. При стабильной работе: `SENSOR_DEBUG_OSD 0`
