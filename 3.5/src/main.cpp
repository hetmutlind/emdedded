#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  POT_PIN          = 34;

    static constexpr uint8_t  SERVO_PIN        = 25;
    static constexpr uint8_t  SERVO_CHANNEL    = 0;

    static constexpr uint32_t SERVO_FREQ_HZ    = 50;
    static constexpr uint8_t  SERVO_RESOLUTION = 16;

    static constexpr uint32_t US_TO_TICKS      = 65536UL / 20000UL;

    static constexpr uint16_t SERVO_MIN_US     = 500;
    static constexpr uint16_t SERVO_MAX_US     = 2500;
    static constexpr uint16_t SERVO_MIN_DEG    = 0;
    static constexpr uint16_t SERVO_MAX_DEG    = 180;

    static constexpr int16_t  POT_RANGE_MIN_DEG = -10;
    static constexpr int16_t  POT_RANGE_MAX_DEG = 200;

    static constexpr uint16_t ADC_MAX          = 4095;

    static constexpr uint32_t SAMPLE_PERIOD_MS = 50;
    static constexpr uint32_t LOG_PERIOD_MS    = 200;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

class Servo {
public:
    Servo(uint8_t pin, uint8_t channel)
        : pin_(pin), channel_(channel), currentDeg_(0) {}

    void init() {
        ledcSetup(channel_, Config::SERVO_FREQ_HZ, Config::SERVO_RESOLUTION);
        ledcAttachPin(pin_, channel_);
        writeDeg(0);
    }

    void writeDeg(int16_t deg) {
        const int16_t clamped = constrain(deg, Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);
        currentDeg_ = clamped;

        const uint32_t pulseUs =
            map(clamped,
                Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG,
                Config::SERVO_MIN_US,  Config::SERVO_MAX_US);

        const uint32_t duty = pulseUs * Config::US_TO_TICKS;
        ledcWrite(channel_, duty);
    }

    int16_t deg() const { return currentDeg_; }

private:
    const uint8_t pin_;
    const uint8_t channel_;
    int16_t       currentDeg_;

    Servo(const Servo&)            = delete;
    Servo& operator=(const Servo&) = delete;
};

static Servo gServo(Config::SERVO_PIN, Config::SERVO_CHANNEL);

static constexpr int16_t kEffMin =
    (Config::POT_RANGE_MIN_DEG > Config::SERVO_MIN_DEG)
    ? (int16_t)Config::POT_RANGE_MIN_DEG
    : (int16_t)Config::SERVO_MIN_DEG;
static constexpr int16_t kEffMax =
    (Config::POT_RANGE_MAX_DEG < Config::SERVO_MAX_DEG)
    ? (int16_t)Config::POT_RANGE_MAX_DEG
    : (int16_t)Config::SERVO_MAX_DEG;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] Module 3.5 — Servo PWM Control"));
    Serial.print(F("[INFO] Pot pin          : GPIO"));
    Serial.println(Config::POT_PIN);
    Serial.print(F("[INFO] Servo pin        : GPIO"));
    Serial.println(Config::SERVO_PIN);
    Serial.print(F("[INFO] Servo PWM        : "));
    Serial.print(Config::SERVO_FREQ_HZ);
    Serial.print(F(" Hz, "));
    Serial.print(Config::SERVO_RESOLUTION);
    Serial.println(F("-bit"));
    Serial.print(F("[INFO] Servo range      : "));
    Serial.print(Config::SERVO_MIN_DEG);
    Serial.print(F("° – "));
    Serial.print(Config::SERVO_MAX_DEG);
    Serial.println(F("°"));
    Serial.print(F("[INFO] Pot mapped range : "));
    Serial.print(Config::POT_RANGE_MIN_DEG);
    Serial.print(F("° – "));
    Serial.print(Config::POT_RANGE_MAX_DEG);
    Serial.println(F("°"));
    Serial.print(F("[INFO] Effective range  : "));
    Serial.print(kEffMin);
    Serial.print(F("° – "));
    Serial.print(kEffMax);
    Serial.println(F("°  (intersection, clamped)"));
    Serial.println();

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    gServo.init();

    Serial.println(F("--- Ready ---"));
    Serial.println(F("ADC    POT_DEG   SERVO_DEG   DEVIATION   PULSE_US"));
    Serial.println(F("---------------------------------------------------"));
}

void loop() {
    static uint32_t lastSampleMs = 0;
    static uint32_t lastLogMs    = 0;

    const uint32_t now = millis();
    if (now - lastSampleMs < Config::SAMPLE_PERIOD_MS) return;
    lastSampleMs = now;

    const uint16_t adc = analogRead(Config::POT_PIN);

    const int16_t potDeg = (int16_t)map(
        adc, 0, Config::ADC_MAX,
        Config::POT_RANGE_MIN_DEG, Config::POT_RANGE_MAX_DEG);

    const int16_t servoDeg = constrain(potDeg, kEffMin, kEffMax);


    const int16_t deviation = servoDeg - kEffMin;

    gServo.writeDeg(servoDeg);

    if (now - lastLogMs >= Config::LOG_PERIOD_MS) {
        lastLogMs = now;

        const uint32_t pulseUs = map(
            servoDeg,
            Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG,
            Config::SERVO_MIN_US,  Config::SERVO_MAX_US);

        char buf[64];
        snprintf(buf, sizeof(buf),
            "%4u   %7d°   %9d°   %9d°   %7lu µs",
            adc, potDeg, servoDeg, deviation,
            (unsigned long)pulseUs);
        Serial.println(buf);
    }
}
