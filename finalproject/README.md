# Smart Clock for ESP32 + ESP-IDF

Це базовий проєкт для смарт-годинника на ESP-IDF/CMake з:
- SPI OLED/SSD1306/SH1106 дисплеєм через U8g2
- BME280 через I2C
- DS1307 RTC через I2C
- двома кнопками, енкодером і LED

## Підключення

- I2C: SDA=21, SCL=22
- SPI display: SCLK=18, MOSI=23, CS=5, DC=16, RESET=17
- LEDs: GPIO19, GPIO4
- Buttons: GPIO34, GPIO35, GPIO25
- Encoder: GPIO32, GPIO33

## Збірка

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

> Якщо у вас інший дисплей або інші GPIO, змініть константи в main/main.cpp.
