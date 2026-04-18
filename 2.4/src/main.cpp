#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  BUTTON_PIN      = 0;
    static constexpr uint32_t DEBOUNCE_MS     = 50;
    static constexpr uint32_t POLL_INTERVAL_MS = 10;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

namespace Task1 {
    static volatile uint32_t gCount = 0;

    void IRAM_ATTR isr() { ++gCount; }

    void setup() {
        pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(Config::BUTTON_PIN), isr, FALLING);
    }

    void loop() {
        static uint32_t last = 0;
        const uint32_t  cur  = gCount;
        if (cur != last) {
            last = cur;
            Serial.print(F("[T1] count="));
            Serial.println(cur);
        }
    }
}

namespace Task2 {
    static volatile bool     gFired = false;
    static volatile uint32_t gCount = 0;

    void IRAM_ATTR isr() { gFired = true; }

    void setup() {
        pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(Config::BUTTON_PIN), isr, FALLING);
    }

    void loop() {
        static uint32_t lastMs = 0;
        if (!gFired) return;
        gFired = false;
        const uint32_t now = millis();
        if ((now - lastMs) < Config::DEBOUNCE_MS) return;
        lastMs = now;
        ++gCount;
        Serial.print(F("[T2] count="));
        Serial.println(gCount);
    }
}

namespace Task3 {
    static volatile bool     gFired = false;
    static volatile uint32_t gCount = 0;

    void IRAM_ATTR isr() { gFired = true; }

    void setup() {
        pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(Config::BUTTON_PIN), isr, FALLING);
    }

    void loop() {
        if (!gFired) return;
        gFired = false;
        if (digitalRead(Config::BUTTON_PIN) == HIGH) return;
        ++gCount;
        Serial.print(F("[T3] count="));
        Serial.println(gCount);
    }
}

namespace Task4 {
    enum class BtnFsm : uint8_t { Idle, Pressed, Released };

    static uint32_t gCount = 0;

    void setup() {
        pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
    }

    void loop() {
        static uint32_t lastPollMs  = 0;
        static uint32_t debounceMs  = 0;
        static BtnFsm   state       = BtnFsm::Idle;
        static bool     lastRaw     = true;

        const uint32_t now = millis();
        if ((now - lastPollMs) < Config::POLL_INTERVAL_MS) return;
        lastPollMs = now;

        const bool raw = (digitalRead(Config::BUTTON_PIN) == HIGH);

        switch (state) {
            case BtnFsm::Idle:
                if (!raw) { debounceMs = now; state = BtnFsm::Pressed; }
                break;

            case BtnFsm::Pressed:
                if (raw) { state = BtnFsm::Idle; break; }
                if ((now - debounceMs) >= Config::DEBOUNCE_MS) {
                    ++gCount;
                    Serial.print(F("[T4] count="));
                    Serial.println(gCount);
                    state = BtnFsm::Released;
                }
                break;

            case BtnFsm::Released:
                if (raw) { state = BtnFsm::Idle; }
                break;
        }

        lastRaw = raw;
    }
}

static constexpr uint8_t ACTIVE_TASK = 4;

void setup() {
    Serial.begin(115200);
    Serial.print(F("[BOOT] Active task: "));
    Serial.println(ACTIVE_TASK);

    if      (ACTIVE_TASK == 1) Task1::setup();
    else if (ACTIVE_TASK == 2) Task2::setup();
    else if (ACTIVE_TASK == 3) Task3::setup();
    else if (ACTIVE_TASK == 4) Task4::setup();
}

void loop() {
    if      (ACTIVE_TASK == 1) Task1::loop();
    else if (ACTIVE_TASK == 2) Task2::loop();
    else if (ACTIVE_TASK == 3) Task3::loop();
    else if (ACTIVE_TASK == 4) Task4::loop();
}