#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  POT_PIN        = 34;

    static constexpr uint8_t  LED_PIN        = 25;
    static constexpr uint8_t  MOTOR_PIN      = 26;

    static constexpr uint8_t  LED_CHANNEL    = 0;
    static constexpr uint8_t  MOTOR_CHANNEL  = 1;

    static constexpr uint32_t PWM_FREQ_HZ    = 5000;
    static constexpr uint8_t  PWM_RESOLUTION = 8;

    static constexpr uint16_t ADC_MAX        = 4095;
    static constexpr uint8_t  DUTY_MAX       = (1 << PWM_RESOLUTION) - 1;

    static constexpr uint32_t SAMPLE_PERIOD_MS = 100;
    static constexpr uint32_t LOG_INTERVAL_MS  = 500;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

class PwmOutput {
public:
    PwmOutput(uint8_t pin, uint8_t channel, uint32_t freqHz, uint8_t resolutionBits)
        : pin_(pin), channel_(channel), freqHz_(freqHz), resolutionBits_(resolutionBits) {}

    void init() const {
        ledcSetup(channel_, freqHz_, resolutionBits_);
        ledcAttachPin(pin_, channel_);
        ledcWrite(channel_, 0);
    }

    void setDuty(uint8_t duty) const {
        ledcWrite(channel_, duty);
    }

private:
    const uint8_t  pin_;
    const uint8_t  channel_;
    const uint32_t freqHz_;
    const uint8_t  resolutionBits_;

    PwmOutput(const PwmOutput&)            = delete;
    PwmOutput& operator=(const PwmOutput&) = delete;
};

static PwmOutput gLed(Config::LED_PIN,   Config::LED_CHANNEL,   Config::PWM_FREQ_HZ, Config::PWM_RESOLUTION);
static PwmOutput gMotor(Config::MOTOR_PIN, Config::MOTOR_CHANNEL, Config::PWM_FREQ_HZ, Config::PWM_RESOLUTION);

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] Module 3.3 — PWM: LED + Motor via Potentiometer"));
    Serial.print(F("[INFO] Pot pin       : GPIO"));
    Serial.println(Config::POT_PIN);
    Serial.print(F("[INFO] LED pin       : GPIO"));
    Serial.print(Config::LED_PIN);
    Serial.print(F("  (channel "));
    Serial.print(Config::LED_CHANNEL);
    Serial.println(F(")"));
    Serial.print(F("[INFO] Motor pin     : GPIO"));
    Serial.print(Config::MOTOR_PIN);
    Serial.print(F("  (channel "));
    Serial.print(Config::MOTOR_CHANNEL);
    Serial.println(F(")"));
    Serial.print(F("[INFO] PWM frequency : "));
    Serial.print(Config::PWM_FREQ_HZ);
    Serial.println(F(" Hz"));
    Serial.print(F("[INFO] PWM resolution: "));
    Serial.print(Config::PWM_RESOLUTION);
    Serial.println(F("-bit (0-255)"));
    Serial.println();

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    gLed.init();
    gMotor.init();

    Serial.println(F("--- Ready ---"));
    Serial.println(F("ADC    DUTY   LED%    MOTOR%"));
    Serial.println(F("------------------------------------"));
}

void loop() {
    static uint32_t lastSampleMs = 0;
    static uint32_t lastLogMs    = 0;
    const  uint32_t now = millis();

    if (now - lastSampleMs >= Config::SAMPLE_PERIOD_MS) {
        lastSampleMs = now;

        const uint16_t adc  = analogRead(Config::POT_PIN);
        const uint8_t  duty = (static_cast<uint32_t>(adc) * Config::DUTY_MAX) / Config::ADC_MAX;

        gLed.setDuty(duty);
        gMotor.setDuty(duty);

        if (now - lastLogMs >= Config::LOG_INTERVAL_MS) {
            lastLogMs = now;

            const float pct = (duty * 100.0f) / Config::DUTY_MAX;

            char buf[40];
            snprintf(buf, sizeof(buf), "%4u   %4u   %5.1f%%   %5.1f%%",
                     adc, duty, pct, pct);
            Serial.println(buf);
        }
    }
}
