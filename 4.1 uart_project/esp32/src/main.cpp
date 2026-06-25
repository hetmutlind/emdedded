#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  BTN_PIN       = 0;
    static constexpr uint8_t  LED_PIN       = 2;

    static constexpr uint32_t UART_BAUD     = 115200;
    static constexpr uint8_t  CMD_TOGGLE    = 0xA1;
    static constexpr uint8_t  CMD_ACK       = 0xB1;

    static constexpr uint32_t DEBOUNCE_MS   = 50;
    static constexpr uint32_t LOG_PERIOD_MS = 2000;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

class Button {
public:
    explicit Button(uint8_t pin)
        : pin_(pin), lastState_(HIGH), stableState_(HIGH), lastMs_(0) {}

    void init() const { pinMode(pin_, INPUT_PULLUP); }

    bool pressed() {
        const uint8_t  raw = digitalRead(pin_);
        const uint32_t now = millis();
        if (raw != lastState_) { lastState_ = raw; lastMs_ = now; }
        if ((now - lastMs_) >= Config::DEBOUNCE_MS && raw != stableState_) {
            stableState_ = raw;
            if (stableState_ == LOW) return true;
        }
        return false;
    }

private:
    const uint8_t pin_;
    uint8_t  lastState_;
    uint8_t  stableState_;
    uint32_t lastMs_;
};

class Led {
public:
    explicit Led(uint8_t pin) : pin_(pin), state_(false) {}
    void init()   const { pinMode(pin_, OUTPUT); digitalWrite(pin_, LOW); }
    void toggle()       { state_ = !state_; digitalWrite(pin_, state_ ? HIGH : LOW); }
    bool state()  const { return state_; }
private:
    const uint8_t pin_;
    bool state_;
};

class UartBridge {
public:
    void init() const {
        Serial2.begin(Config::UART_BAUD);
    }

    void sendToggle() const {
        Serial2.write(Config::CMD_TOGGLE);
        Serial.println(F("[UART TX] 0xA1 → STM32 toggle LED"));
    }

    bool receive(uint8_t &cmd) const {
        if (!Serial2.available()) return false;
        cmd = Serial2.read();
        return true;
    }
};

static Button     gBtn(Config::BTN_PIN);
static Led        gLed(Config::LED_PIN);
static UartBridge gUart;

static uint32_t   gTxCount = 0;
static uint32_t   gRxCount = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] UART bridge — ESP32 side"));
    Serial.println(F("[INFO] TX2=GPIO17 → STM32 PA10 (USART1 RX)"));
    Serial.println(F("[INFO] RX2=GPIO16 ← STM32 PA9  (USART1 TX)"));
    Serial.print(F("[INFO] Baud rate : "));
    Serial.println(Config::UART_BAUD);
    Serial.println(F("[INFO] Protocol : 0xA1=toggle  0xB1=ACK"));
    Serial.println();

    gBtn.init();
    gLed.init();
    gUart.init();

    Serial.println(F("--- Ready ---"));
    Serial.println(F("[BTN]  Press BOOT to toggle STM32 LED"));
    Serial.println();
}

void loop() {
    if (gBtn.pressed()) {
        gUart.sendToggle();
        ++gTxCount;
    }

    uint8_t cmd = 0;
    if (gUart.receive(cmd)) {
        if (cmd == Config::CMD_TOGGLE) {
            gLed.toggle();
            ++gRxCount;
            Serial.print(F("[UART RX] 0xA1 ← STM32 button → LED "));
            Serial.println(gLed.state() ? F("ON") : F("OFF"));

            Serial2.write(Config::CMD_ACK);
            Serial.println(F("[UART TX] 0xB1 → STM32 ACK"));
        } else if (cmd == Config::CMD_ACK) {
            Serial.println(F("[UART RX] 0xB1 ← STM32 ACK"));
        } else {
            Serial.print(F("[UART RX] unknown byte: 0x"));
            Serial.println(cmd, HEX);
        }
    }

    static uint32_t lastHbMs = 0;
    const  uint32_t now = millis();
    if (now - lastHbMs >= Config::LOG_PERIOD_MS) {
        lastHbMs = now;
        Serial.print(F("[HB] uptime="));
        Serial.print(now / 1000);
        Serial.print(F("s  tx="));
        Serial.print(gTxCount);
        Serial.print(F("  rx="));
        Serial.print(gRxCount);
        Serial.print(F("  led="));
        Serial.println(gLed.state() ? F("ON") : F("OFF"));
    }
}
