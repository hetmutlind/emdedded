/*
 * Mini-project refactor
 *
 * LDR (ADC1_CH8 / GPIO9) → SMA filter → servo angle (LEDC, 50 Hz)
 *
 * What changed vs original:
 *  - All magic numbers moved to named constants
 *  - Debug printf() calls removed; replaced with ESP_LOGI
 *  - set_angle() split into pure mapping helpers + single actuator call
 *  - SMA filter added (window = SMA_WINDOW)
 *  - adc_oneshot_read() result checked with ESP_ERROR_CHECK
 *  - Commented-out dead code removed
 *  - Handles and calibration state encapsulated in structs
 *  - SERVO_PERIOD_MS macro fixed (needs parentheses: (1000/SERVO_FREQ))
 *  - duty formula uses the actual SERVO_MAX_DUTY instead of magic 4095/20
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "MiniProject";

// ─── Config ─────────────────────────────────────────────────
#define ADC_CHANNEL_LDR     ADC_CHANNEL_8       // GPIO9
#define ADC_UNIT_LDR        ADC_UNIT_1
#define ADC_ATTEN_LDR       ADC_ATTEN_DB_12
#define ADC_BITWIDTH_LDR    ADC_BITWIDTH_12

#define SERVO_GPIO          14
#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_MS     (1000 / SERVO_FREQ_HZ)   // 20 ms
#define SERVO_RESOLUTION    LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY      ((1 << 12) - 1)           // 4095
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL_IDX   LEDC_CHANNEL_0

#define SMA_WINDOW          10
#define LOOP_PERIOD_MS      100

// Calibration — adjust to match physical servo and LDR range
#define SERVO_MIN_DEG       10
#define SERVO_MAX_DEG       170
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500
#define LIGHT_MIN_MV        50
#define LIGHT_MAX_MV        3000

// ─── SMA state ──────────────────────────────────────────────
typedef struct {
    int     buf[SMA_WINDOW];
    uint8_t idx;
    int     sum;
    uint8_t count;
} sma_t;

static void sma_init(sma_t *s) {
    memset(s, 0, sizeof(*s));
}

static int sma_update(sma_t *s, int sample) {
    if (s->count == SMA_WINDOW) {
        s->sum -= s->buf[s->idx];
    } else {
        s->count++;
    }
    s->buf[s->idx] = sample;
    s->sum        += sample;
    s->idx         = (s->idx + 1) % SMA_WINDOW;
    return s->sum / s->count;
}

// ─── ADC / LDR ──────────────────────────────────────────────
typedef struct {
    adc_oneshot_unit_handle_t unit;
    adc_cali_handle_t         cali;
} ldr_t;

static void ldr_init(ldr_t *ldr) {
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_LDR };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &ldr->unit));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_LDR,
        .bitwidth = ADC_BITWIDTH_LDR,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ldr->unit, ADC_CHANNEL_LDR, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_LDR,
        .chan     = ADC_CHANNEL_LDR,
        .atten    = ADC_ATTEN_LDR,
        .bitwidth = ADC_BITWIDTH_LDR,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &ldr->cali));
}

static int ldr_read_mv(const ldr_t *ldr) {
    int raw = 0, mv = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(ldr->unit, ADC_CHANNEL_LDR, &raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ldr->cali, raw, &mv));
    return mv;
}

// ─── Servo ──────────────────────────────────────────────────
static void servo_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_TIMER,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = SERVO_CHANNEL_IDX,
        .timer_sel  = SERVO_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = SERVO_GPIO,
        .duty       = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

// ─── Helpers ────────────────────────────────────────────────
static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Linear map with clamped input
static int range_map(int v, int in_lo, int in_hi, int out_lo, int out_hi) {
    if (in_lo == in_hi) return out_hi;
    v = clamp(v, in_lo, in_hi);
    return out_lo + (v - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

static uint32_t us_to_duty(int us) {
    // duty/SERVO_MAX_DUTY = us/SERVO_PERIOD_MS/1000
    // → duty = SERVO_MAX_DUTY * us / (SERVO_PERIOD_MS * 1000)
    return (uint32_t)SERVO_MAX_DUTY * (uint32_t)us / (SERVO_PERIOD_MS * 1000);
}

static void servo_write_mv(int mv) {
    mv = clamp(mv, LIGHT_MIN_MV, LIGHT_MAX_MV);

    const int deg  = range_map(mv,  LIGHT_MIN_MV,  LIGHT_MAX_MV,  SERVO_MIN_DEG, SERVO_MAX_DEG);
    const int us   = range_map(deg, SERVO_MIN_DEG, SERVO_MAX_DEG, SERVO_MIN_US,  SERVO_MAX_US);
    const uint32_t duty = us_to_duty(us);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL_IDX, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL_IDX));

    ESP_LOGI(TAG, "mv=%4d  deg=%3d  us=%4d  duty=%4lu", mv, deg, us, (unsigned long)duty);
}

// ─── app_main ───────────────────────────────────────────────
void app_main(void) {
    ESP_LOGI(TAG, "=== BOOT ===");
    ESP_LOGI(TAG, "LDR channel : ADC1_CH8 (GPIO9)");
    ESP_LOGI(TAG, "Servo GPIO  : %d", SERVO_GPIO);
    ESP_LOGI(TAG, "SMA window  : %d", SMA_WINDOW);
    ESP_LOGI(TAG, "Loop period : %d ms", LOOP_PERIOD_MS);

    ldr_t ldr;
    ldr_init(&ldr);
    servo_init();

    sma_t sma;
    sma_init(&sma);

    ESP_LOGI(TAG, "--- Ready ---");

    while (1) {
        const int mv_raw      = ldr_read_mv(&ldr);
        const int mv_filtered = sma_update(&sma, mv_raw);

        servo_write_mv(mv_filtered);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}
