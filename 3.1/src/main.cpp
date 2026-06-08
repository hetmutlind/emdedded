#include <Arduino.h>
#include <esp_adc_cal.h>

static constexpr uint8_t  ADC_PIN        = 34;
static constexpr adc_attenuation_t ATTENUATION = ADC_11db;
static constexpr uint32_t VREF_MV        = 3300;
static constexpr uint8_t  ADC_BITS       = 12;
static constexpr uint32_t ADC_MAX        = (1 << ADC_BITS) - 1;
static constexpr uint32_t SAMPLE_MS      = 100;

static esp_adc_cal_characteristics_t gAdcChars;


void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println(F("\n[BOOT] Module 3 Lesson 4 – ADC Calibration"));
    Serial.println(F("[INFO] ADC pin     : GPIO34 (ADC1_CH6)"));
    Serial.println(F("[INFO] Attenuation : 11 dB  (0 – 3300 mV)"));
    Serial.println(F("[INFO] Resolution  : 12 bit (0 – 4095)"));
    Serial.println(F("[INFO] Vref        : 3300 mV"));
    Serial.println(F("[INFO] Interval    : 100 ms\n"));

    analogReadResolution(ADC_BITS);
    analogSetAttenuation(ATTENUATION);

    esp_adc_cal_value_t calType = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_12,
        ADC_WIDTH_BIT_12,
        1100,
        &gAdcChars
    );

    if (calType == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.println(F("[CAL]  Source: eFuse Vref"));
    } else if (calType == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.println(F("[CAL]  Source: eFuse Two-Point"));
    } else {
        Serial.println(F("[CAL]  Source: Default Vref (1100 mV)"));
    }

    Serial.println();
    Serial.println(F("RAW    U_manual(mV)   U_cali(mV)   Error(%)"));
    Serial.println(F("------------------------------------------------"));
}

void loop() {
    static uint32_t lastMs = 0;
    const  uint32_t now    = millis();

    if (now - lastMs < SAMPLE_MS) return;
    lastMs = now;

    const uint32_t raw = analogRead(ADC_PIN);

    const float uManual = (float)raw * VREF_MV / ADC_MAX;

    const uint32_t uCali = esp_adc_cal_raw_to_voltage(raw, &gAdcChars);

    float error = 0.0f;
    if (uCali > 0) {
        error = fabsf(uManual - (float)uCali) / (float)uCali * 100.0f;
    }

    char buf[64];
    snprintf(buf, sizeof(buf),
        "%4lu   %9.1f      %6lu       %6.2f",
        (unsigned long)raw,
        uManual,
        (unsigned long)uCali,
        error
    );
    Serial.println(buf);
}
