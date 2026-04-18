#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  LED1_PIN      = 25;
    static constexpr uint8_t  LED2_PIN      = 26;
    static constexpr uint8_t  LED3_PIN      = 27;
    static constexpr uint32_t LED1_INTERVAL = 200;
    static constexpr uint32_t LED2_INTERVAL = 500;
    static constexpr uint32_t LED3_INTERVAL = 1000;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

enum class LedState : uint8_t { Off = LOW, On = HIGH };

class Led {
public:
    explicit Led(uint8_t pin)
        : pin_(pin), state_(LedState::Off), lastMs_(0), interval_(0) {}

    void init(uint32_t intervalMs) {
        interval_ = intervalMs;
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, static_cast<uint8_t>(LedState::Off));
    }

    void tick() {
        const uint32_t now = millis();
        if ((now - lastMs_) >= interval_) {
            lastMs_ = now;
            state_  = (state_ == LedState::Off) ? LedState::On : LedState::Off;
            digitalWrite(pin_, static_cast<uint8_t>(state_));
        }
    }

    LedState state() const { return state_; }

private:
    const uint8_t pin_;
    LedState      state_;
    uint32_t      lastMs_;
    uint32_t      interval_;

    Led(const Led&)            = delete;
    Led& operator=(const Led&) = delete;
};

static Led gLed1(Config::LED1_PIN);
static Led gLed2(Config::LED2_PIN);
static Led gLed3(Config::LED3_PIN);

void setup() {
    Serial.begin(115200);
    gLed1.init(Config::LED1_INTERVAL);
    gLed2.init(Config::LED2_INTERVAL);
    gLed3.init(Config::LED3_INTERVAL);
}

void loop() {
    gLed1.tick();
    gLed2.tick();
    gLed3.tick();
}