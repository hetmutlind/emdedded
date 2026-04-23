#include <Arduino.h>
#include <cmath>

#define LDR_PIN    1
#define SAMPLES   10
#define PAUSE_MS 100
#define ADC_MAX  4095
#define VREF_MV  3300

static float calcMean(const int* arr, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += static_cast<float>(arr[i]);
    return sum / static_cast<float>(n);
}

static float calcStd(const int* arr, int n, float mean) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        float d = static_cast<float>(arr[i]) - mean;
        sum += d * d;
    }
    return std::sqrt(sum / static_cast<float>(n));
}

void setup() {
    Serial.begin(115200);
    delay(800);

    analogReadResolution(12);
    analogSetPinAttenuation(LDR_PIN, ADC_11db);

    Serial.println();
    Serial.println("LDR ADC Measurement  (12-bit, ADC_11db, Vref=3300 mV)");
    Serial.println("------------------------------------------------------------");
    Serial.println(" #  | RAW  | Vcalc (mV) | Vmv (mV) | Err (mV) | Err (%)");
    Serial.println("----+------+------------+----------+----------+----------");

    int rawArr[SAMPLES];
    int  mvArr[SAMPLES];

    for (int i = 0; i < SAMPLES; ++i) {
        rawArr[i] = analogRead(LDR_PIN);
        mvArr[i]  = analogReadMillivolts(LDR_PIN);

        const float vcalc = rawArr[i] * static_cast<float>(VREF_MV) / ADC_MAX;
        const float err   = vcalc - static_cast<float>(mvArr[i]);
        const float errPct = (mvArr[i] > 0)
                             ? (err / static_cast<float>(mvArr[i]) * 100.0f)
                             : 0.0f;

        Serial.printf(
            " %2d | %4d | %10.1f | %8d | %+8.1f | %+7.2f%%\n",
            i + 1, rawArr[i], vcalc, mvArr[i], err, errPct
        );

        delay(PAUSE_MS);
    }

    Serial.println("----+------+------------+----------+----------+----------");

    const float rawMean = calcMean(rawArr, SAMPLES);
    const float rawStd  = calcStd(rawArr, SAMPLES, rawMean);
    const float mvMean  = calcMean(mvArr,  SAMPLES);
    const float vcMean  = rawMean * static_cast<float>(VREF_MV) / ADC_MAX;
    const float errMv   = vcMean - mvMean;
    const float errPct  = (mvMean > 0.0f) ? (errMv / mvMean * 100.0f) : 0.0f;

    Serial.println();
    Serial.printf("RAW  mean: %.1f  std: %.2f\n", rawMean, rawStd);
    Serial.printf("Vcalc mean: %.1f mV\n", vcMean);
    Serial.printf("Vmv   mean: %.1f mV\n", mvMean);
    Serial.printf("Error mean: %+.1f mV  (%+.2f%%)\n", errMv, errPct);
    Serial.println("------------------------------------------------------------");
}

void loop() {}
