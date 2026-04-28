#include <Arduino.h>
#include <stdint.h>
#include "esp_task_wdt.h"

// ─── Config ───────────────────────────────────────────────
class Config {
public:
    static constexpr uint8_t  FAN_PIN          = 26;
    static constexpr uint8_t  LED_FAN_PIN      = 25;
    static constexpr uint8_t  LED_IDLE_PIN     = 27;

    static constexpr uint64_t PERIOD_US        = 10ULL * 1000000ULL;
    static constexpr uint64_t ON_TIME_US       = 4ULL  * 1000000ULL;

    static constexpr uint32_t WDT_TIMEOUT_S    = 5;
    static constexpr uint32_t LOG_INTERVAL_MS  = 1000;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

// ─── Fan state ────────────────────────────────────────────
enum class FanState : uint8_t { Idle, Running };

// ─── Logger ───────────────────────────────────────────────
class Logger {
public:
    static void log(const char* msg) {
        Serial.print(F("["));
        Serial.print(millis());
        Serial.print(F(" ms] "));
        Serial.println(msg);
    }

    static void logState(FanState s, uint64_t nextEventUs) {
        static uint32_t lastMs = 0;
        const  uint32_t now    = millis();
        if ((now - lastMs) < Config::LOG_INTERVAL_MS) return;
        lastMs = now;

        const uint64_t  nowUs  = static_cast<uint64_t>(esp_timer_get_time());
        const uint64_t  leftUs = (nextEventUs > nowUs) ? (nextEventUs - nowUs) : 0;

        Serial.print(F("["));
        Serial.print(now);
        Serial.print(F(" ms] state="));
        Serial.print(s == FanState::Running ? F("RUNNING") : F("IDLE   "));
        Serial.print(F("  next_event_in="));
        Serial.print(static_cast<uint32_t>(leftUs / 1000));
        Serial.println(F(" ms"));
    }

    Logger()                       = delete;
    Logger(const Logger&)          = delete;
    Logger& operator=(const Logger&) = delete;
};

// ─── Fan controller ───────────────────────────────────────
class FanController {
public:
    FanController()
        : state_(FanState::Idle)
        , nextEventUs_(0) {}

    void init() {
        pinMode(Config::FAN_PIN,      OUTPUT);
        pinMode(Config::LED_FAN_PIN,  OUTPUT);
        pinMode(Config::LED_IDLE_PIN, OUTPUT);

        applyState(FanState::Idle);

        nextEventUs_ = static_cast<uint64_t>(esp_timer_get_time())
                       + Config::PERIOD_US - Config::ON_TIME_US;
        started_     = false;

        Logger::log("FanController init: waiting for first cycle");
    }

    void tick() {
        const uint64_t now = static_cast<uint64_t>(esp_timer_get_time());

        if (now < nextEventUs_) {
            Logger::logState(state_, nextEventUs_);
            return;
        }

        switch (state_) {
            case FanState::Idle:
                applyState(FanState::Running);
                nextEventUs_ = now + Config::ON_TIME_US;
                Logger::log("Fan ON");
                break;

            case FanState::Running:
                applyState(FanState::Idle);
                nextEventUs_ = now + (Config::PERIOD_US - Config::ON_TIME_US);
                Logger::log("Fan OFF — next cycle scheduled");
                break;
        }

        Logger::logState(state_, nextEventUs_);
    }

    FanState state() const { return state_; }

private:
    FanState state_;
    uint64_t nextEventUs_;

    void applyState(FanState s) {
        state_ = s;
        const uint8_t fanLevel  = (s == FanState::Running) ? HIGH : LOW;
        const uint8_t idleLevel = (s == FanState::Idle)    ? HIGH : LOW;
        digitalWrite(Config::FAN_PIN,      fanLevel);
        digitalWrite(Config::LED_FAN_PIN,  fanLevel);
        digitalWrite(Config::LED_IDLE_PIN, idleLevel);
    }

    FanController(const FanController&)            = delete;
    FanController& operator=(const FanController&) = delete;
};

// ─── Timer callback ───────────────────────────────────────
static FanController gFan;
static esp_timer_handle_t gTimer = nullptr;

static void IRAM_ATTR timerCb(void* /*arg*/) {
    gFan.tick();
}

// ─── Setup ────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Logger::log("BOOT");

    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms        = Config::WDT_TIMEOUT_S * 1000,
        .idle_core_mask    = 0,
        .trigger_panic     = true
    };
    esp_task_wdt_reconfigure(&wdtCfg);
    esp_task_wdt_add(nullptr);

    gFan.init();

    const esp_timer_create_args_t args = {
        .callback        = timerCb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "fan_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&args, &gTimer);
    esp_timer_start_periodic(gTimer, 100000ULL);

    Logger::log("Timer started (100 ms tick)");
}

// ─── Loop ─────────────────────────────────────────────────
void loop() {
    esp_task_wdt_reset();
}