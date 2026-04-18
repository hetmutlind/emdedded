#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    // Піни
    static constexpr uint8_t  LED_PIN           = 2;  
    static constexpr uint8_t  BUTTON_PIN        = 0;    

    static constexpr uint32_t BLINK_INTERVAL_MS = 500; 
    static constexpr uint32_t DEBOUNCE_MS       = 50;  

    static const uint8_t      BLINKS_ON_PRESS; 

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

const uint8_t Config::BLINKS_ON_PRESS = 3;

enum class LedState : uint8_t {
    Off = LOW,
    On  = HIGH
};

enum class LedMode : uint8_t {
    Blinking  = 0,
    AlwaysOn  = 1,
    AlwaysOff = 2,
    _COUNT    = 3   
};

class Led {
public:
    explicit Led(uint8_t pin)
        : pin_(pin), state_(LedState::Off) {}

    void init() const {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, static_cast<uint8_t>(LedState::Off));
    }

    void set(LedState s) {
        state_ = s;
        digitalWrite(pin_, static_cast<uint8_t>(s));
    }

    LedState state() const { return state_; }

private:
    const uint8_t pin_;
    LedState      state_;

    Led(const Led&)            = delete;
    Led& operator=(const Led&) = delete;
};

static Led           gLed(Config::LED_PIN);
static LedMode       gMode          = LedMode::Blinking;
static volatile bool gButtonPressed = false; 

void IRAM_ATTR onButtonPress() {
    gButtonPressed = true;
}

static void nextMode() {
    const uint8_t next = (static_cast<uint8_t>(gMode) + 1U)
                         % static_cast<uint8_t>(LedMode::_COUNT);
    gMode = static_cast<LedMode>(next);

    static const char* const kModeNames[] = {
        "Blinking", "AlwaysOn", "AlwaysOff"
    };
    Serial.print(F("[MODE] -> "));
    Serial.println(kModeNames[next]);
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("[BOOT] ESP32 Embedded C++ Blink"));

    gLed.init();

    pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Config::BUTTON_PIN),
                    onButtonPress, FALLING);
}

void loop() {
    static uint32_t sLoopCount = 0;
    static uint32_t sLastLogMs = 0;
    static uint32_t sMaxIterUs = 0;

    const uint32_t iterStart = micros();
    ++sLoopCount;

    static uint32_t sLastDebounceMs = 0;

    if (gButtonPressed) {
        const uint32_t now = millis();
        if ((now - sLastDebounceMs) >= Config::DEBOUNCE_MS) {
            sLastDebounceMs = now;
            nextMode();
        }
        gButtonPressed = false;        
    }

    static uint32_t sLastBlinkMs = 0;

    switch (gMode) {
        case LedMode::Blinking: {
            const uint32_t now = millis();
            if ((now - sLastBlinkMs) >= Config::BLINK_INTERVAL_MS) {
                sLastBlinkMs = now;
                gLed.set(gLed.state() == LedState::Off
                             ? LedState::On
                             : LedState::Off);
            }
            break;
        }
        case LedMode::AlwaysOn:
            gLed.set(LedState::On);
            break;

        case LedMode::AlwaysOff:
            gLed.set(LedState::Off);
            break;

        default:
            break;
    }

    const uint32_t iterUs = micros() - iterStart;
    if (iterUs > sMaxIterUs) { sMaxIterUs = iterUs; }

    const uint32_t nowMs = millis();
    if ((nowMs - sLastLogMs) >= 1000UL) {
        sLastLogMs = nowMs;
        Serial.print(F("[PERF] iter/s ~"));
        Serial.print(sLoopCount);
        Serial.print(F("  max iter us: "));
        Serial.println(sMaxIterUs);
        sLoopCount = 0;
        sMaxIterUs = 0;
    }
}