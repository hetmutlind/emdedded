#include <Arduino.h>
#include <stdint.h>

class Config {
public:
    static constexpr uint8_t  BUZZER_PIN      = 25;
    static constexpr uint8_t  PWM_CHANNEL     = 0;
    static constexpr uint8_t  PWM_RESOLUTION  = 8;
    static constexpr uint8_t  PWM_DUTY        = 127;

    static constexpr uint32_t TICK_MS         = 50;
    static constexpr uint32_t LOG_INTERVAL_MS = 200;

    Config()                         = delete;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;
};

namespace Note {
    static constexpr uint16_t REST = 0;
    static constexpr uint16_t C4   = 262;
    static constexpr uint16_t D4   = 294;
    static constexpr uint16_t E4   = 330;
    static constexpr uint16_t F4   = 349;
    static constexpr uint16_t G4   = 392;
    static constexpr uint16_t A4   = 440;
    static constexpr uint16_t B4   = 494;
    static constexpr uint16_t C5   = 523;
    static constexpr uint16_t D5   = 587;
    static constexpr uint16_t E5   = 659;
    static constexpr uint16_t G5   = 784;
}

namespace Dur {
    static constexpr uint8_t Q  = 4;
    static constexpr uint8_t H  = 8;
    static constexpr uint8_t DQ = 6;
    static constexpr uint8_t E  = 2;
}

struct NoteEntry {
    uint16_t freq;
    uint8_t  durationTicks;
};

static const NoteEntry kMelody[] = {
    { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::H },
    { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::H },
    { Note::E4, Dur::Q }, { Note::G4, Dur::Q }, { Note::C4, Dur::Q }, { Note::D4, Dur::Q },
    { Note::E4, Dur::H }, { Note::REST, Dur::H },

    { Note::F4, Dur::Q }, { Note::F4, Dur::Q }, { Note::F4, Dur::DQ }, { Note::F4, Dur::E },
    { Note::F4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::E }, { Note::E4, Dur::E },

    { Note::E4, Dur::Q }, { Note::D4, Dur::Q }, { Note::D4, Dur::Q }, { Note::E4, Dur::Q },
    { Note::D4, Dur::H }, { Note::G4, Dur::H },

    { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::H },
    { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::H },
    { Note::E4, Dur::Q }, { Note::G4, Dur::Q }, { Note::C4, Dur::Q }, { Note::D4, Dur::Q },
    { Note::E4, Dur::H }, { Note::REST, Dur::H },

    { Note::F4, Dur::Q }, { Note::F4, Dur::Q }, { Note::F4, Dur::DQ }, { Note::F4, Dur::E },
    { Note::F4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::Q }, { Note::E4, Dur::E }, { Note::E4, Dur::E },

    { Note::G4, Dur::Q }, { Note::G4, Dur::Q }, { Note::F4, Dur::Q }, { Note::D4, Dur::Q },
    { Note::C4, Dur::H }, { Note::REST, Dur::H },
};

static constexpr uint8_t kMelodyLen = sizeof(kMelody) / sizeof(kMelody[0]);

class Buzzer {
public:
    explicit Buzzer(uint8_t pin, uint8_t channel, uint8_t resolution)
        : pin_(pin), channel_(channel), resolution_(resolution) {}

    void init() const {
        ledcSetup(channel_, 1000, resolution_);
        ledcAttachPin(pin_, channel_);
        ledcWrite(channel_, 0);
    }

    void play(uint16_t freq) const {
        if (freq == Note::REST) {
            ledcWrite(channel_, 0);
        } else {
            ledcChangeFrequency(channel_, freq, resolution_);
            ledcWrite(channel_, Config::PWM_DUTY);
        }
    }

    void stop() const { ledcWrite(channel_, 0); }

private:
    const uint8_t pin_;
    const uint8_t channel_;
    const uint8_t resolution_;

    Buzzer(const Buzzer&)            = delete;
    Buzzer& operator=(const Buzzer&) = delete;
};

class Sequencer {
public:
    Sequencer(const NoteEntry* score, uint8_t len, Buzzer& buzzer)
        : score_(score), len_(len), buzzer_(buzzer),
          noteIdx_(0), ticksLeft_(0), ticksTotal_(0) {}

    void tick() {
        if (ticksLeft_ == 0) {
            noteIdx_ = (noteIdx_ + 1) % len_;
            const NoteEntry& n = score_[noteIdx_];
            ticksLeft_ = n.durationTicks;
            ticksTotal_++;
            buzzer_.play(n.freq);

            Serial.print(F("[NOTE] "));
            Serial.print(noteName(n.freq));
            Serial.print(F("  freq="));
            Serial.print(n.freq);
            Serial.print(F(" Hz  dur="));
            Serial.print(static_cast<uint32_t>(n.durationTicks) * Config::TICK_MS);
            Serial.println(F(" ms"));
        }
        --ticksLeft_;
    }

    uint8_t noteIdx()   const { return noteIdx_; }
    uint16_t curFreq()  const { return score_[noteIdx_].freq; }

private:
    static const char* noteName(uint16_t freq) {
        switch (freq) {
            case Note::C4:   return "C4 ";
            case Note::D4:   return "D4 ";
            case Note::E4:   return "E4 ";
            case Note::F4:   return "F4 ";
            case Note::G4:   return "G4 ";
            case Note::A4:   return "A4 ";
            case Note::B4:   return "B4 ";
            case Note::C5:   return "C5 ";
            case Note::D5:   return "D5 ";
            case Note::E5:   return "E5 ";
            case Note::G5:   return "G5 ";
            case Note::REST: return "REST";
            default:         return "??? ";
        }
    }

    const NoteEntry* score_;
    const uint8_t    len_;
    Buzzer&          buzzer_;
    uint8_t          noteIdx_;
    uint8_t          ticksLeft_;
    uint32_t         ticksTotal_;

    Sequencer(const Sequencer&)            = delete;
    Sequencer& operator=(const Sequencer&) = delete;
};

static Buzzer     gBuzzer(Config::BUZZER_PIN, Config::PWM_CHANNEL, Config::PWM_RESOLUTION);
static Sequencer  gSeq(kMelody, kMelodyLen, gBuzzer);

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] Module 3.4 — Buzzer PWM Player"));
    Serial.print(F("[INFO] Buzzer pin    : GPIO"));
    Serial.println(Config::BUZZER_PIN);
    Serial.print(F("[INFO] PWM channel   : "));
    Serial.println(Config::PWM_CHANNEL);
    Serial.print(F("[INFO] Tick interval : "));
    Serial.print(Config::TICK_MS);
    Serial.println(F(" ms"));
    Serial.print(F("[INFO] Melody        : Jingle Bells ("));
    Serial.print(kMelodyLen);
    Serial.println(F(" notes, looping)"));
    Serial.println(F("[INFO] Mode         : non-blocking, no FreeRTOS"));
    Serial.println();

    gBuzzer.init();

    Serial.println(F("--- Playing ---"));
    Serial.println();
}

void loop() {
    static uint32_t lastTickMs = 0;
    const  uint32_t now = millis();

    if (now - lastTickMs >= Config::TICK_MS) {
        lastTickMs = now;
        gSeq.tick();
    }
}
