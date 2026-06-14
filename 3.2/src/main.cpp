#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  LDR_PIN         = 34;
    static constexpr uint8_t  LED_PIN         = 2;

    static constexpr uint8_t  SMA_WINDOW_SIZE = 10;
    static constexpr uint32_t SAMPLE_PERIOD_MS = 50;

    static constexpr int      THRESHOLD_DARK  = 1800;
    static constexpr int      THRESHOLD_LIGHT = 2200;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

class SmaFilter {
public:
    explicit SmaFilter(uint8_t windowSize)
        : windowSize_(windowSize), index_(0), sum_(0), count_(0) {
        buffer_ = new int[windowSize_];
        for (uint8_t i = 0; i < windowSize_; ++i) buffer_[i] = 0;
    }

    ~SmaFilter() { delete[] buffer_; }

    int update(int newSample) {
        if (count_ == windowSize_) {
            sum_ -= buffer_[index_];
        } else {
            ++count_;
        }

        buffer_[index_] = newSample;
        sum_ += newSample;

        index_ = (index_ + 1) % windowSize_;

        return sum_ / count_;
    }

private:
    int*    buffer_;
    uint8_t windowSize_;
    uint8_t index_;
    long    sum_;
    uint8_t count_;

    SmaFilter(const SmaFilter&)            = delete;
    SmaFilter& operator=(const SmaFilter&) = delete;
};

enum class LedState : uint8_t { Off, On };

class HysteresisLed {
public:
    explicit HysteresisLed(uint8_t pin, int threshDark, int threshLight)
        : pin_(pin), threshDark_(threshDark), threshLight_(threshLight),
          state_(LedState::Off) {}

    void init() const {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    bool update(int filteredValue) {
        LedState newState = state_;

        if (state_ == LedState::Off && filteredValue < threshDark_) {
            newState = LedState::On;
        } else if (state_ == LedState::On && filteredValue > threshLight_) {
            newState = LedState::Off;
        }

        if (newState != state_) {
            state_ = newState;
            digitalWrite(pin_, state_ == LedState::On ? HIGH : LOW);
            return true;
        }
        return false;
    }

    LedState state() const { return state_; }

private:
    const uint8_t pin_;
    const int     threshDark_;
    const int     threshLight_;
    LedState      state_;

    HysteresisLed(const HysteresisLed&)            = delete;
    HysteresisLed& operator=(const HysteresisLed&) = delete;
};

static SmaFilter      gSma(Config::SMA_WINDOW_SIZE);
static HysteresisLed  gLed(Config::LED_PIN, Config::THRESHOLD_DARK, Config::THRESHOLD_LIGHT);

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] Module 3.2 — SMA LDR Light Sensor"));
    Serial.print(F("[INFO] LDR pin       : GPIO"));
    Serial.println(Config::LDR_PIN);
    Serial.print(F("[INFO] LED pin       : GPIO"));
    Serial.println(Config::LED_PIN);
    Serial.print(F("[INFO] SMA window    : "));
    Serial.println(Config::SMA_WINDOW_SIZE);
    Serial.print(F("[INFO] Sample period : "));
    Serial.print(Config::SAMPLE_PERIOD_MS);
    Serial.println(F(" ms"));
    Serial.print(F("[INFO] Threshold DARK : "));
    Serial.println(Config::THRESHOLD_DARK);
    Serial.print(F("[INFO] Threshold LIGHT: "));
    Serial.println(Config::THRESHOLD_LIGHT);
    Serial.println();

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    gLed.init();

    Serial.println(F("--- Ready ---"));
    Serial.println(F("RAW    SMA    LED"));
    Serial.println(F("------------------------"));
}

void loop() {
    static uint32_t lastMs = 0;
    const  uint32_t now    = millis();
    if (now - lastMs < Config::SAMPLE_PERIOD_MS) return;
    lastMs = now;

    const int raw      = analogRead(Config::LDR_PIN);
    const int filtered = gSma.update(raw);

    const bool changed = gLed.update(filtered);

    char buf[32];
    snprintf(buf, sizeof(buf), "%4d   %4d   %s",
             raw, filtered,
             gLed.state() == LedState::On ? "ON " : "OFF");
    Serial.println(buf);

    if (changed) {
        Serial.print(F(">>> LED switched to "));
        Serial.print(gLed.state() == LedState::On ? F("ON") : F("OFF"));
        Serial.print(F("  (filtered="));
        Serial.print(filtered);
        Serial.println(F(")"));
    }
}
