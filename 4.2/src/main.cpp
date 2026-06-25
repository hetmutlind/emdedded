#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

class Config {
public:
    static constexpr uint8_t  SCREEN_W      = 128;
    static constexpr uint8_t  SCREEN_H      = 64;
    static constexpr int8_t   OLED_RESET    = -1;  
    static constexpr uint8_t  OLED_ADDR     = 0x3C;

    static constexpr uint32_t UPDATE_MS     = 1000;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

static const char* const kDayNames[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static Adafruit_SSD1306 gDisplay(Config::SCREEN_W, Config::SCREEN_H,
                                  &Wire, Config::OLED_RESET);
static RTC_DS1307 gRtc;
static bool       gRtcOk = false;

static bool display_init() {
    if (!gDisplay.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDR)) {
        Serial.println(F("[OLED] begin() failed — check wiring / address"));
        return false;
    }
    gDisplay.clearDisplay();
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.cp437(true);
    Serial.println(F("[OLED] OK"));
    return true;
}

static void display_error(const char* msg) {
    gDisplay.clearDisplay();
    gDisplay.setTextSize(1);
    gDisplay.setCursor(0, 24);
    gDisplay.println(msg);
    gDisplay.display();
}

static void display_update(const char* time_str, const char* date_str) {
    gDisplay.clearDisplay();

    gDisplay.setTextSize(3);

    gDisplay.setTextSize(2);
    gDisplay.setCursor(16, 14);
    gDisplay.print(time_str);

    gDisplay.drawFastHLine(0, 38, 128, SSD1306_WHITE);

    gDisplay.setTextSize(1);
    gDisplay.setCursor(22, 48);
    gDisplay.print(date_str);

    gDisplay.display();
}

static bool rtc_init() {
    if (!gRtc.begin(&Wire)) {
        Serial.println(F("[RTC] DS1307 not found — check wiring"));
        return false;
    }
    if (!gRtc.isrunning()) {
        Serial.println(F("[RTC] not running — setting compile time"));
        gRtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.println(F("[RTC] OK"));
    return true;
}

static void rtc_read(DateTime& dt, char* time_buf, char* date_buf) {
    dt = gRtc.now();

    snprintf(time_buf, 16, "%02u:%02u:%02u",
             dt.hour(), dt.minute(), dt.second());

    const char* day = (dt.dayOfTheWeek() < 7)
                      ? kDayNames[dt.dayOfTheWeek()]
                      : "???";

    snprintf(date_buf, 16, "%s %02u.%02u.%04u",
             day, dt.day(), dt.month(), dt.year());
}

static void log_time(const char* time_buf, const char* date_buf) {
    Serial.print(F("[TIME] "));
    Serial.print(date_buf);
    Serial.print(F("  "));
    Serial.println(time_buf);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] I2C Clock — SSD1306 + DS1307"));
    Serial.println(F("[INFO] SDA=GPIO21  SCL=GPIO22"));
    Serial.println(F("[INFO] OLED 0x3C   RTC 0x68"));
    Serial.println();

    Wire.begin();

    const bool oledOk = display_init();
    gRtcOk            = rtc_init();

    if (!oledOk) {
        Serial.println(F("[FATAL] display init failed — halting"));
        while (true) { delay(1000); }
    }

    if (!gRtcOk) {
        display_error("RTC error");
        Serial.println(F("[WARN] running without RTC"));
    }

    Serial.println(F("--- Ready ---"));
}

void loop() {
    static uint32_t lastMs = 0;
    const  uint32_t now    = millis();
    if (now - lastMs < Config::UPDATE_MS) return;
    lastMs = now;

    if (!gRtcOk) {
        display_error("RTC error\ncheck wiring");
        return;
    }

    DateTime dt;
    char time_buf[16];
    char date_buf[16];

    rtc_read(dt, time_buf, date_buf);
    display_update(time_buf, date_buf);
    log_time(time_buf, date_buf);
}
