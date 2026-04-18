#include <Arduino.h>
#include <stdint.h>
#include <stdatomic.h>

// ─── Config ───────────────────────────────────────────────
class Config {
public:
    static constexpr uint8_t  PIN_RED    = 25;
    static constexpr uint8_t  PIN_YELLOW = 26;
    static constexpr uint8_t  PIN_GREEN  = 27;

    static constexpr uint32_t T_GREEN_MS        = 5000;
    static constexpr uint32_t T_GREEN_BLINK_MS  = 3000;
    static constexpr uint32_t T_BLINK_HALF_MS   = 500;
    static constexpr uint32_t T_YELLOW_MS        = 2000;
    static constexpr uint32_t T_RED_MS           = 5000;
    static constexpr uint32_t T_RED_YELLOW_MS    = 2000;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

// ─── Phase ────────────────────────────────────────────────
enum class Phase : uint8_t {
    Green        = 0,
    GreenBlink   = 1,
    Yellow       = 2,
    Red          = 3,
    RedYellow    = 4
};

// ─── Atomic phase ───
static atomic_uint_fast32_t gPhaseAtomic = ATOMIC_VAR_INIT(
    static_cast<uint_fast32_t>(Phase::Green)
);

// ─── Led helpers ──────────────────────────────────────────
static inline void ledSet(uint8_t pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW);
}

static void applyLeds(bool r, bool y, bool g) {
    ledSet(Config::PIN_RED,    r);
    ledSet(Config::PIN_YELLOW, y);
    ledSet(Config::PIN_GREEN,  g);
}

// ─── Traffic light controller ─────────────────────────────
class TrafficLight {
public:
    TrafficLight()
        : phase_(Phase::Green)
        , phaseStart_(0)
        , blinkStart_(0)
        , blinkOn_(true) {}

    void init() {
        pinMode(Config::PIN_RED,    OUTPUT);
        pinMode(Config::PIN_YELLOW, OUTPUT);
        pinMode(Config::PIN_GREEN,  OUTPUT);

        phaseStart_ = millis();
        blinkStart_ = phaseStart_;
        enterPhase(Phase::Green);
    }

    void tick() {
        const uint32_t now     = millis();
        const uint32_t elapsed = now - phaseStart_;

        switch (phase_) {
            case Phase::Green:
                if (elapsed >= Config::T_GREEN_MS)
                    enterPhase(Phase::GreenBlink);
                break;

            case Phase::GreenBlink: {
                if ((now - blinkStart_) >= Config::T_BLINK_HALF_MS) {
                    blinkStart_ = now;
                    blinkOn_    = !blinkOn_;
                    applyLeds(false, false, blinkOn_);
                }
                if (elapsed >= Config::T_GREEN_BLINK_MS)
                    enterPhase(Phase::Yellow);
                break;
            }

            case Phase::Yellow:
                if (elapsed >= Config::T_YELLOW_MS)
                    enterPhase(Phase::Red);
                break;

            case Phase::Red:
                if (elapsed >= Config::T_RED_MS)
                    enterPhase(Phase::RedYellow);
                break;

            case Phase::RedYellow:
                if (elapsed >= Config::T_RED_YELLOW_MS)
                    enterPhase(Phase::Green);
                break;
        }
    }

    Phase phase() const { return phase_; }

private:
    Phase    phase_;
    uint32_t phaseStart_;
    uint32_t blinkStart_;
    bool     blinkOn_;

    void enterPhase(Phase p) {
        phase_      = p;
        phaseStart_ = millis();
        blinkStart_ = phaseStart_;
        blinkOn_    = true;

        atomic_store_explicit(
            &gPhaseAtomic,
            static_cast<uint_fast32_t>(p),
            memory_order_release
        );

        switch (p) {
            case Phase::Green:      applyLeds(false, false, true);  break;
            case Phase::GreenBlink: applyLeds(false, false, true);  break;
            case Phase::Yellow:     applyLeds(false, true,  false); break;
            case Phase::Red:        applyLeds(true,  false, false); break;
            case Phase::RedYellow:  applyLeds(true,  true,  false); break;
        }

        static const char* const kNames[] = {
            "GREEN", "GREEN_BLINK", "YELLOW", "RED", "RED+YELLOW"
        };
        Serial.print(F("[TL] phase -> "));
        Serial.println(kNames[static_cast<uint8_t>(p)]);
    }

    TrafficLight(const TrafficLight&)            = delete;
    TrafficLight& operator=(const TrafficLight&) = delete;
};

// ─── Globals ──────────────────────────────────────────────
static TrafficLight gTL;

// ─── Setup / Loop ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println(F("[BOOT] UA Traffic Light"));
    gTL.init();
}

void loop() {
    gTL.tick();
}