#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  RELAY_CTRL_PIN   = 26;
    static constexpr uint8_t  RELAY_FB_PIN     = 27;
    static constexpr uint8_t  POT_PIN          = 34;
    static constexpr uint8_t  MOTOR_PIN        = 25;
    static constexpr uint8_t  MEASUREMENTS     = 10;
    static constexpr uint32_t RELAY_TIMEOUT_MS = 100;
    static constexpr uint32_t MEASURE_DELAY_MS = 500;
    static constexpr uint32_t PWM_PERIOD_US    = 20000;
    static constexpr uint16_t ADC_MAX          = 4095;
    static constexpr uint32_t BOOT_DELAY_MS    = 3000;

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

private:
    const uint8_t ctrlPin_;
    const uint8_t fbPin_;

    Relay(const Relay&)            = delete;
    Relay& operator=(const Relay&) = delete;
};


static volatile bool     gCaptured        = false;
static volatile uint32_t gCaptureMillis   = 0;

void IRAM_ATTR onRelayFeedback() {
    gCaptureMillis = millis();
    gCaptured      = true;
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
        dutyUs_ = min(dutyUs, periodUs_);
    }

    void tick() {
        const uint32_t now     = micros();
        const uint32_t elapsed = now - lastUs_;

        if (high_) {
            if (elapsed >= dutyUs_) {
                digitalWrite(pin_, LOW);
                high_   = false;
                lastUs_ = now;
            }
        } else {
            if (elapsed >= periodUs_ - dutyUs_) {
                if (dutyUs_ > 0) digitalWrite(pin_, HIGH);
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

enum class AppState : uint8_t {
    Boot,
    Measuring,
    Waiting,
    Reporting,
    RunPwm
};

static Relay   gRelay(Config::RELAY_CTRL_PIN, Config::RELAY_FB_PIN);
static SoftPwm gPwm(Config::MOTOR_PIN, Config::PWM_PERIOD_US);

static AppState gState          = AppState::Boot;
static uint8_t  gCount          = 0;
static uint32_t gMeasurements[Config::MEASUREMENTS];
static uint32_t gStateEnteredMs = 0;
static uint32_t gRelayOnMs      = 0;

void setup() {
    Serial.begin(115200);

    gRelay.init();
    gPwm.init();

    attachInterrupt(digitalPinToInterrupt(Config::RELAY_FB_PIN),
                    onRelayFeedback, RISING);

    gStateEnteredMs = millis();
    Serial.println(F("[BOOT] Relay + SoftPWM demo"));
    Serial.println(F("Open Serial Monitor (115200 baud)..."));
}

void loop() {
    const uint32_t now = millis();

    switch (gState) {

    case AppState::Boot:
        if (now - gStateEnteredMs >= Config::BOOT_DELAY_MS) {
            Serial.println(F("--- Measuring relay activation time ---"));
            gState          = AppState::Measuring;
            gStateEnteredMs = now;

            gCaptured   = false;
            gRelayOnMs  = millis();
            gRelay.set(RelayState::On);
        }
        break;

    case AppState::Measuring:
        if (gCaptured) {
            const uint32_t elapsed = gCaptureMillis - gRelayOnMs;
            gRelay.set(RelayState::Off);

            gMeasurements[gCount] = elapsed;
            Serial.print(F("  #"));
            Serial.print(gCount + 1);
            Serial.print(F(": "));
            Serial.print(elapsed);
            Serial.println(F("ms"));

            ++gCount;
            gState          = (gCount < Config::MEASUREMENTS)
                              ? AppState::Waiting
                              : AppState::Reporting;
            gStateEnteredMs = now;

        } else if (now - gRelayOnMs >= Config::RELAY_TIMEOUT_MS) {
            gRelay.set(RelayState::Off);
            Serial.println(F("  timeout, skipping"));
            gState          = AppState::Waiting;
            gStateEnteredMs = now;
        }
        break;

    case AppState::Waiting:
        if (now - gStateEnteredMs >= Config::MEASURE_DELAY_MS) {
            if (gCount < Config::MEASUREMENTS) {
                gCaptured  = false;
                gRelayOnMs = millis();
                gRelay.set(RelayState::On);
                gState     = AppState::Measuring;
            } else {
                gState = AppState::Reporting;
            }
            gStateEnteredMs = now;
        }
        break;

    case AppState::Reporting: {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < gCount; ++i) { sum += gMeasurements[i]; }
        const uint32_t avg = gCount ? (sum / gCount) : 0;

        Serial.println(F("-----------------------------------------"));
        Serial.print(F("Success: "));  Serial.println(gCount);
        Serial.print(F("Average:          "));  Serial.print(avg);
        Serial.println(F(" ms"));
        Serial.println(F("-----------------------------------------"));
        Serial.println(F("[PWM] Manage motor speed with the potentiometer"));

        detachInterrupt(digitalPinToInterrupt(Config::RELAY_FB_PIN));
        gState = AppState::RunPwm;
        break;
    }

    case AppState::RunPwm: {
        const uint16_t adc  = analogRead(Config::POT_PIN);
        const uint32_t duty = (static_cast<uint32_t>(adc) * Config::PWM_PERIOD_US)
                              / Config::ADC_MAX;
        gPwm.setDuty(duty);
        gPwm.tick();
        break;
    }
    }
}