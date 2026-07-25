#include <Arduino.h>
#include <stdint.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <ESP32Servo.h>  

class Config {
public:
    static constexpr uint8_t  ENC_CLK        = 36;
    static constexpr uint8_t  ENC_DT         = 35;
    static constexpr uint8_t  ENC_BTN        = 37;  

    static constexpr uint8_t  SERVO_PIN      = 19;
    static constexpr uint8_t  SERVO_CHANNEL  = 0;   
    static constexpr uint16_t SERVO_MIN_US   = 500;
    static constexpr uint16_t SERVO_MAX_US   = 2500;
    static constexpr int16_t  SERVO_MIN_DEG  = 0;
    static constexpr int16_t  SERVO_MAX_DEG  = 180;
    static constexpr int16_t  SERVO_CENTER   = 90;

    static constexpr uint8_t  BUZZER_PIN     = 26;
    static constexpr uint8_t  BUZZER_CHANNEL = 1;
    static constexpr uint32_t BEEP_FREQ_HZ   = 1500;
    static constexpr uint32_t BEEP_MS        = 80;

    static constexpr uint8_t  STEP_COARSE    = 5;
    static constexpr uint8_t  STEP_MID       = 2;
    static constexpr uint8_t  STEP_FINE      = 1;

    static constexpr uint32_t DEBOUNCE_MS    = 40;
    static constexpr uint32_t LONG_PRESS_MS  = 800;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

class Buzzer {
public:
    Buzzer(uint8_t pin, uint8_t channel)
        : pin_(pin), channel_(channel), untilMs_(0) {}

    void init() const {
        ledcSetup(channel_, Config::BEEP_FREQ_HZ, 8);
        ledcAttachPin(pin_, channel_);
        ledcWrite(channel_, 0);
    }

    void beep(uint32_t durationMs = Config::BEEP_MS) {
        ledcWrite(channel_, 127);
        untilMs_ = millis() + durationMs;
    }

    void tick() {
        if (untilMs_ && millis() >= untilMs_) {
            ledcWrite(channel_, 0);
            untilMs_ = 0;
        }
    }

private:
    const uint8_t pin_;
    const uint8_t channel_;
    uint32_t      untilMs_;
};

class Encoder {
public:
    Encoder(uint8_t clk, uint8_t dt, uint8_t btn)
        : clkPin_(clk), dtPin_(dt), btnPin_(btn),
          lastClk_(HIGH), delta_(0),
          btnState_(HIGH), lastBtnMs_(0),
          pressedMs_(0), longFired_(false) {}

    void init() const {
        pinMode(clkPin_, INPUT);
        pinMode(dtPin_,  INPUT);
        pinMode(btnPin_, INPUT_PULLUP);
    }

    void tick() {
        const uint8_t clk = digitalRead(clkPin_);
        if (clk != lastClk_ && clk == LOW) {
            delta_ += (digitalRead(dtPin_) != clk) ? +1 : -1;
        }
        lastClk_ = clk;

        const uint8_t raw = digitalRead(btnPin_);
        const uint32_t now = millis();
        if (raw != btnState_ && (now - lastBtnMs_) >= Config::DEBOUNCE_MS) {
            btnState_ = raw;
            lastBtnMs_ = now;
            if (raw == LOW) {
                pressedMs_ = now;
                longFired_ = false;
            }
        }
        if (btnState_ == LOW && !longFired_ &&
            (millis() - pressedMs_) >= Config::LONG_PRESS_MS) {
            longFired_ = true;
            longPressEvent_ = true;
        }
    }

    int8_t  consumeDelta()     { const int8_t d = delta_; delta_ = 0; return d; }
    bool    consumeShortPress() {
        if (btnState_ == HIGH && lastBtnMs_ > 0 && !longFired_ &&
            (millis() - lastBtnMs_) < Config::LONG_PRESS_MS + 50) {
            if (!shortConsumed_) { shortConsumed_ = true; return true; }
        }
        if (btnState_ == HIGH) shortConsumed_ = false;
        return false;
    }
    bool    consumeLongPress()  { const bool v = longPressEvent_; longPressEvent_ = false; return v; }

private:
    const uint8_t clkPin_, dtPin_, btnPin_;
    uint8_t  lastClk_;
    int8_t   delta_;
    uint8_t  btnState_;
    uint32_t lastBtnMs_;
    uint32_t pressedMs_;
    bool     longFired_;
    bool     longPressEvent_  = false;
    bool     shortConsumed_   = false;
};

static Servo   gServo;  
static Buzzer  gBuzzer(Config::BUZZER_PIN, Config::BUZZER_CHANNEL);
static Encoder gEncoder(Config::ENC_CLK, Config::ENC_DT, Config::ENC_BTN);

static const uint8_t kSteps[]    = { Config::STEP_COARSE, Config::STEP_MID, Config::STEP_FINE };
static const char*   kStepNames[] = { "COARSE (5°)", "MID (2°)", "FINE (1°)" };
static constexpr uint8_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

static uint8_t gStepIdx = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_bt_controller_disable();

    Serial.println(F("\n[BOOT] Module 3.6 — Encoder Servo Control (ESP32Servo)"));
    Serial.print(F("[INFO] Encoder pins : CLK="));
    Serial.print(Config::ENC_CLK);
    Serial.print(F(" DT="));
    Serial.print(Config::ENC_DT);
    Serial.print(F(" BTN="));
    Serial.println(Config::ENC_BTN);
    Serial.print(F("[INFO] Servo pin    : GPIO"));
    Serial.println(Config::SERVO_PIN);
    Serial.print(F("[INFO] Buzzer pin   : GPIO"));
    Serial.println(Config::BUZZER_PIN);
    Serial.print(F("[INFO] Step default : "));
    Serial.print(Config::STEP_COARSE);
    Serial.println(F("° / tick"));
    Serial.println(F("[INFO] Long press   : center servo to 90°"));
    Serial.println();

    gBuzzer.init();

    gEncoder.init();

    gServo.setPeriodHertz(50); 
    gServo.attach(Config::SERVO_PIN, Config::SERVO_MIN_US, Config::SERVO_MAX_US);
    gServo.write(Config::SERVO_CENTER); 
    Serial.println(F("--- Ready ---"));
    Serial.print(F("[STEP] "));
    Serial.println(kStepNames[gStepIdx]);
    Serial.print(F("[POS]  "));
    Serial.print(gServo.read());
    Serial.println(F("°  (center)"));
    Serial.println();
}

void loop() {
    gEncoder.tick();
    gBuzzer.tick();

    const int8_t delta = gEncoder.consumeDelta();
    if (delta != 0) {
        const int16_t step = kSteps[gStepIdx] * (delta > 0 ? 1 : -1);
        int16_t current = gServo.read();
        int16_t target = current + step;
        target = constrain(target, Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);

        if (target == current) {
            gBuzzer.beep();
            Serial.print(F("[LIMIT] "));
            Serial.print(delta > 0 ? F("MAX") : F("MIN"));
            Serial.println(F(" reached — beep"));
        } else {
            gServo.write(target);
            const int16_t dev = target - Config::SERVO_MIN_DEG;
            Serial.print(F("[POS]  "));
            Serial.print(target);
            Serial.print(F("°  (dev from 0°: "));
            Serial.print(dev);
            Serial.println(F("°)"));
        }
    }

    if (gEncoder.consumeShortPress()) {
        gStepIdx = (gStepIdx + 1) % kStepCount;
        Serial.print(F("[STEP] "));
        Serial.println(kStepNames[gStepIdx]);
    }

    if (gEncoder.consumeLongPress()) {
        gServo.write(Config::SERVO_CENTER);
        Serial.println(F("[CENTER] Long press → servo → 90°"));
    }
}