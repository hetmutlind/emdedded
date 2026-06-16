# Mini-project — Module 3 Refactor

LDR (ADC1_CH8 / GPIO9) → SMA filter → Servo (LEDC, 50 Hz, GPIO14)

## Requirements

- ESP-IDF v5.2+
- Target: ESP32-S3

## Build

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Wiring

| Signal | GPIO |
|--------|------|
| LDR out (voltage divider) | 9 |
| Servo signal | 14 |

## Config

Edit constants at the top of `main/main.c`:

| Constant | Default | Description |
|----------|---------|-------------|
| `LIGHT_MIN_MV` | 50 | LDR voltage at full dark |
| `LIGHT_MAX_MV` | 3000 | LDR voltage at full bright |
| `SERVO_MIN_DEG` | 10 | Servo leftmost angle |
| `SERVO_MAX_DEG` | 170 | Servo rightmost angle |
| `SMA_WINDOW` | 10 | Moving average window size |
| `LOOP_PERIOD_MS` | 100 | Superloop period |
