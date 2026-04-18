#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  RELAY_CTRL_PIN    = 26;
    static constexpr uint8_t  RELAY_FB_PIN      = 27;
    static constexpr uint8_t  POT_PIN           = 34;
    static constexpr uint8_t  MOTOR_PIN         = 25;
    static constexpr uint8_t  MEASUREMENTS      = 10;
    static constexpr uint32_t RELAY_PULSE_US    = 100000;
    static constexpr uint32_t MEASURE_DELAY_MS  = 500;
    static constexpr uint32_t PWM_PERIOD_US     = 20000;
    static constexpr uint16_t ADC_MAX           = 4095;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

enum class RelayState : uint8_t { Off = LOW, On = HIGH };

class Relay {
public:
    Relay(uint8_t ctrlPin, uint8_t fbPin)
        : ctrlPin_(ctrlPin), fbPin_(fbPin) {}

    void init() const {
        pinMode(ctrlPin_, OUTPUT);
        pinMode(fbPin_, INPUT);
        digitalWrite(ctrlPin_, static_cast<uint8_t>(RelayState::Off));
    }

    void set(RelayState s) const {
        digitalWrite(ctrlPin_, static_cast<uint8_t>(s));
    }

    bool feedback() const {
        return digitalRead(fbPin_) == HIGH;
    }

private:
    const uint8_t ctrlPin_;
    const uint8_t fbPin_;

    Relay(const Relay&)            = delete;
    Relay& operator=(const Relay&) = delete;
};

static volatile uint32_t gRisingTimestamp  = 0;
static volatile bool     gCaptured         = false;

void IRAM_ATTR onRelayFeedback() {
    gRisingTimestamp = micros();
    gCaptured        = true;
}

static Relay gRelay(Config::RELAY_CTRL_PIN, Config::RELAY_FB_PIN);

static void runRelayMeasurement() {
    uint32_t measurements[Config::MEASUREMENTS];
    uint8_t  count = 0;

    Serial.println(F("--- Relay timing measurement ---"));

    while (count < Config::MEASUREMENTS) {
        gCaptured = false;

        uint32_t t0 = micros();
        gRelay.set(RelayState::On);

        uint32_t deadline = micros() + Config::RELAY_PULSE_US;
        while (!gCaptured && (micros() < deadline)) { /* spin */ }

        uint32_t elapsed = gCaptured ? (gRisingTimestamp - t0) : 0;

        gRelay.set(RelayState::Off);

        if (gCaptured) {
            measurements[count] = elapsed;
            Serial.print(F("  #"));
            Serial.print(count + 1);
            Serial.print(F(": "));
            Serial.print(elapsed);
            Serial.println(F(" us"));
            ++count;
        } else {
            Serial.println(F("  timeout, skipping"));
        }

        delay(Config::MEASURE_DELAY_MS);
    }

    uint32_t sum = 0;
    for (uint8_t i = 0; i < Config::MEASUREMENTS; ++i) { sum += measurements[i]; }
    const uint32_t avg = sum / Config::MEASUREMENTS;

    Serial.print(F("Average: "));
    Serial.print(avg);
    Serial.println(F(" us"));
    Serial.println(F("--------------------------------"));
}

class SoftPwm {
public:
    SoftPwm(uint8_t pin, uint32_t periodUs)
        : pin_(pin), periodUs_(periodUs), dutyUs_(0), lastUs_(0), high_(false) {}

    void init() const {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void setDuty(uint32_t dutyUs) {
        if (dutyUs > periodUs_) { dutyUs = periodUs_; }
        dutyUs_ = dutyUs;
    }

    void tick() {
        const uint32_t now     = micros();
        const uint32_t elapsed = now - lastUs_;

        if (high_) {
            if (elapsed >= dutyUs_) {
                digitalWrite(pin_, LOW);
                high_    = false;
                lastUs_  = now;
            }
        } else {
            const uint32_t lowTime = periodUs_ - dutyUs_;
            if (elapsed >= lowTime) {
                if (dutyUs_ > 0) { digitalWrite(pin_, HIGH); }
                high_   = true;
                lastUs_ = now;
            }
        }
    }

private:
    const uint8_t  pin_;
    const uint32_t periodUs_;
    uint32_t       dutyUs_;
    uint32_t       lastUs_;
    bool           high_;

    SoftPwm(const SoftPwm&)            = delete;
    SoftPwm& operator=(const SoftPwm&) = delete;
};

static SoftPwm gPwm(Config::MOTOR_PIN, Config::PWM_PERIOD_US);

void setup() {
    Serial.begin(115200);
    Serial.println(F("[BOOT] Relay + SoftPWM demo"));

    gRelay.init();
    gPwm.init();

    attachInterrupt(digitalPinToInterrupt(Config::RELAY_FB_PIN),
                    onRelayFeedback, RISING);

    runRelayMeasurement();

    detachInterrupt(digitalPinToInterrupt(Config::RELAY_FB_PIN));
}

void loop() {
    const uint16_t adc    = analogRead(Config::POT_PIN);
    const uint32_t duty   = (static_cast<uint32_t>(adc) * Config::PWM_PERIOD_US)
                            / Config::ADC_MAX;
    gPwm.setDuty(duty);
    gPwm.tick();
}