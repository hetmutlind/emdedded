#include <Arduino.h>
#include <stdint.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <ESP32Servo.h> 

class Config {
public:
    static constexpr uint8_t  ENCODER_CLK_PIN  = 36;
    static constexpr uint8_t  ENCODER_DT_PIN   = 35;
    static constexpr uint8_t  ENCODER_SW_PIN   = 0;

    static constexpr uint8_t  SERVO_PIN        = 19; 

    static constexpr uint16_t SERVO_MIN_US     = 500;
    static constexpr uint16_t SERVO_MAX_US     = 2500;
    static constexpr uint16_t SERVO_MIN_DEG    = 0;
    static constexpr uint16_t SERVO_MAX_DEG    = 180;

    static constexpr int16_t  INPUT_RANGE_MIN_DEG = -10;
    static constexpr int16_t  INPUT_RANGE_MAX_DEG = 200;

    static constexpr uint32_t SAMPLE_PERIOD_MS = 10;
    static constexpr uint32_t LOG_PERIOD_MS    = 200;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

static volatile int32_t gEncoderPosition = 0;
static volatile uint8_t  gEncoderState    = 0;

class Encoder {
public:
    static void init() {
        pinMode(Config::ENCODER_CLK_PIN, INPUT_PULLUP);
        pinMode(Config::ENCODER_DT_PIN, INPUT_PULLUP);
        gEncoderState = readPins();
    }

    static void update() {
        uint8_t newState = readPins();
        int8_t delta = 0;
        if (newState != gEncoderState) {
            if ((gEncoderState == 0 && newState == 1) ||
                (gEncoderState == 1 && newState == 3) ||
                (gEncoderState == 3 && newState == 2) ||
                (gEncoderState == 2 && newState == 0)) {
                delta = 1;
            } else if ((gEncoderState == 1 && newState == 0) ||
                       (gEncoderState == 3 && newState == 1) ||
                       (gEncoderState == 2 && newState == 3) ||
                       (gEncoderState == 0 && newState == 2)) {
                delta = -1;
            }
        }
        if (delta != 0) {
            noInterrupts();
            gEncoderPosition += delta;
            interrupts();
        }
        gEncoderState = newState;
    }

    static int32_t position() {
        noInterrupts();
        int32_t value = gEncoderPosition;
        interrupts();
        return value;
    }

private:
    static uint8_t readPins() {
        return (digitalRead(Config::ENCODER_CLK_PIN) << 1) | digitalRead(Config::ENCODER_DT_PIN);
    }
};

static Servo gServo;

static constexpr int16_t kEffMin =
    (Config::INPUT_RANGE_MIN_DEG > Config::SERVO_MIN_DEG)
    ? Config::INPUT_RANGE_MIN_DEG
    : (int16_t)Config::SERVO_MIN_DEG;
static constexpr int16_t kEffMax =
    (Config::INPUT_RANGE_MAX_DEG < Config::SERVO_MAX_DEG)
    ? Config::INPUT_RANGE_MAX_DEG
    : (int16_t)Config::SERVO_MAX_DEG;

void setup() {
    Serial.begin(115200);
    delay(200);

    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_bt_controller_disable();

    Serial.println(F("\n[BOOT] Module 3.5 — Servo PWM Control (ESP32Servo)"));
    Serial.print(F("[INFO] Servo pin         : GPIO"));
    Serial.println(Config::SERVO_PIN);
    Serial.print(F("[INFO] Servo range       : "));
    Serial.print(Config::SERVO_MIN_DEG);
    Serial.print(F("° – "));
    Serial.print(Config::SERVO_MAX_DEG);
    Serial.println(F("°"));
    Serial.print(F("[INFO] Effective range   : "));
    Serial.print(kEffMin);
    Serial.print(F("° – "));
    Serial.print(kEffMax);
    Serial.println(F("°  (intersection, clamped)"));
    Serial.println();

    Encoder::init();

    gServo.setPeriodHertz(50);
    gServo.attach(Config::SERVO_PIN, Config::SERVO_MIN_US, Config::SERVO_MAX_US);
    gServo.write(90);

    Serial.println(F("--- Ready ---"));
    Serial.println(F("POS   INPUT_DEG   SERVO_DEG   DEVIATION   PULSE_US"));
    Serial.println(F("---------------------------------------------------"));
}

void loop() {
    static uint32_t lastSampleMs = 0;
    static uint32_t lastLogMs    = 0;

    const uint32_t now = millis();
    if (now - lastSampleMs < Config::SAMPLE_PERIOD_MS) return;
    lastSampleMs = now;

    Encoder::update();
    const int32_t encoderPos = Encoder::position();
    const int16_t inputDeg = (int16_t)constrain(
        encoderPos,
        Config::INPUT_RANGE_MIN_DEG,
        Config::INPUT_RANGE_MAX_DEG);

    const int16_t servoDeg = constrain(inputDeg, kEffMin, kEffMax);
    const int16_t deviation = servoDeg - kEffMin;

    gServo.write(servoDeg);

    if (now - lastLogMs >= Config::LOG_PERIOD_MS) {
        lastLogMs = now;

        const uint32_t pulseUs = map(
            servoDeg,
            Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG,
            Config::SERVO_MIN_US,  Config::SERVO_MAX_US);

        char buf[64];
        snprintf(buf, sizeof(buf),
            "%5ld   %9d°   %9d°   %9d°   %7lu µs",
            (long)encoderPos, inputDeg, servoDeg, deviation,
            (unsigned long)pulseUs);
        Serial.println(buf);
    }
}