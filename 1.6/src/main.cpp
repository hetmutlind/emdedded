#include <Arduino.h>
#include <cmath>

#define LDR_PIN     1
#define SAMPLES    20    
#define PAUSE_MS  200      


struct AttInfo {
  adc_attenuation_t att;
  const char*       name;
  int               vmax_mv; 
};

static const AttInfo atts[] = {
  { ADC_0db,   "ADC_0db   ",  950  },
  { ADC_2_5db, "ADC_2_5db ", 1250  },
  { ADC_6db,   "ADC_6db   ", 1750  },
  { ADC_11db,  "ADC_11db  ", 3100  },
};

static const int bits[] = { 9, 10, 11, 12 };
static constexpr int nBits = sizeof(bits) / sizeof(bits[0]);
static constexpr int nAtts = sizeof(atts) / sizeof(atts[0]);


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


static void measureCombo(int resBits, const AttInfo& att) {
  analogReadResolution(resBits);
  analogSetPinAttenuation(LDR_PIN, att.att);
  delay(50);  

  const int adcMax = (1 << resBits) - 1;

  int rawArr[SAMPLES];
  int  mvArr[SAMPLES];

  for (int i = 0; i < SAMPLES; ++i) {
    rawArr[i] = analogRead(LDR_PIN);
    mvArr[i]  = analogReadMillivolts(LDR_PIN);
    delay(PAUSE_MS);
  }

  const float rawMean   = calcMean(rawArr, SAMPLES);
  const float rawStd    = calcStd (rawArr, SAMPLES, rawMean);
  const float vCalcMean = rawMean * static_cast<float>(att.vmax_mv)
                                  / static_cast<float>(adcMax);
  const float mvMean    = calcMean(mvArr, SAMPLES);
  const float errMv     = vCalcMean - mvMean;
  const float errPct    = (mvMean > 0.0f) ? (errMv / mvMean * 100.0f) : 0.0f;
  const float lsbMv     = static_cast<float>(att.vmax_mv)
                        / static_cast<float>(adcMax);

  Serial.printf(
    "  %2d біт | %s | %4d мВ | RAW: %6.1f ±%4.1f | "
    "Vкал: %6.1f | Vмв: %6.1f | Err: %+5.1f мВ (%+4.1f%%) | LSB: %.2f мВ\n",
    resBits, att.name, att.vmax_mv,
    rawMean, rawStd,
    vCalcMean, mvMean,
    errMv, errPct,
    lsbMv
  );
}


static void printTheory() {
  Serial.println();
  Serial.println("┌─ Теоретична роздільна здатність (LSB) ──────────────────────────────┐");
  Serial.printf( "│ %-10s ", "Атенюація");
  for (int b : bits) Serial.printf("│ %4d біт  ", b);
  Serial.println("│");
  Serial.println("├──────────────────────────────────────────────────────────────────────┤");

  for (const auto& a : atts) {
    Serial.printf("│ %-10s ", a.name);
    for (int b : bits) {
      const float lsb = static_cast<float>(a.vmax_mv)
                      / static_cast<float>((1 << b) - 1);
      Serial.printf("│ %6.2f мВ ", lsb);
    }
    Serial.println("│");
  }
  Serial.println("└──────────────────────────────────────────────────────────────────────┘");
}


void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║     ESP32-S3 ADC: Порівняння бітності та атенюації (LDR)            ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝");
  Serial.printf ("  Вимірювань на комбінацію: %d\n", SAMPLES);

  printTheory();

  Serial.println();
  Serial.println("─── Результати вимірювань ───────────────────────────────────────────────────────────────");
  Serial.println("  Біт   | Атенюація    | Vmax  | RAW (mid±std)     | Vкал   | Vмв    | Err            | LSB");
  Serial.println("─────────────────────────────────────────────────────────────────────────────────────────────");

  for (int b : bits) {
    for (const auto& a : atts) {
      measureCombo(b, a);
    }
    Serial.println("  ------+──────────────+-------+───────────────────+────────+────────+────────────────+────────");
  }

  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  Serial.println();
  Serial.println("══ Висновок ════════════════════════════════════════════════════════════");
  Serial.println("  Більша бітність  → менший LSB → вища роздільна здатність.");
  Serial.println("  ADC_0db          → найвужчий діапазон (~0..950 мВ), найточніший у ньому.");
  Serial.println("  ADC_11db         → найширший діапазон (~0..3100 мВ), більша нелінійність.");
  Serial.println("  analogReadMillivolts() використовує вбудовану таблицю лінеаризації —");
  Serial.println("  зазвичай точніша за формулу RAW×Vref/adcMax (особливо на краях діапазону).");
  Serial.println("════════════════════════════════════════════════════════════════════════");
}

void loop() {
}
