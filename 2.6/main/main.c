#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

// ─── Config ───────────────────────────────────────────────
#define FAN_PIN           GPIO_NUM_26
#define LED_FAN_PIN       GPIO_NUM_25
#define LED_IDLE_PIN      GPIO_NUM_27

#define PERIOD_US         (10ULL * 1000000ULL)
#define ON_TIME_US        (4ULL  * 1000000ULL)
#define TIMER_TICK_US     (100ULL * 1000ULL)
#define WDT_TIMEOUT_MS    5000

static const char* TAG = "FAN";

// ─── State ────────────────────────────────────────────────
typedef enum {
    FAN_IDLE    = 0,
    FAN_RUNNING = 1
} fan_state_t;

static volatile fan_state_t s_state       = FAN_IDLE;
static volatile uint64_t    s_next_event  = 0;
static esp_timer_handle_t   s_timer       = NULL;

// ─── GPIO helpers ─────────────────────────────────────────
static void gpio_init_all(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << FAN_PIN)
                      | (1ULL << LED_FAN_PIN)
                      | (1ULL << LED_IDLE_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
}

static void apply_state(fan_state_t state) {
    const int fan_lvl  = (state == FAN_RUNNING) ? 1 : 0;
    const int idle_lvl = (state == FAN_IDLE)    ? 1 : 0;
    gpio_set_level(FAN_PIN,      fan_lvl);
    gpio_set_level(LED_FAN_PIN,  fan_lvl);
    gpio_set_level(LED_IDLE_PIN, idle_lvl);
}

// ─── Timer callback ───────────────────────────────────────
static void IRAM_ATTR timer_cb(void* arg) {
    const uint64_t now = (uint64_t)esp_timer_get_time();
    if (now < s_next_event) return;

    if (s_state == FAN_IDLE) {
        s_state      = FAN_RUNNING;
        s_next_event = now + ON_TIME_US;
        apply_state(FAN_RUNNING);
        ESP_DRAM_LOGI(TAG, "Fan ON");
    } else {
        s_state      = FAN_IDLE;
        s_next_event = now + (PERIOD_US - ON_TIME_US);
        apply_state(FAN_IDLE);
        ESP_DRAM_LOGI(TAG, "Fan OFF");
    }
}

// ─── Log task ─────────────────────────────────────────────
static void log_task(void* arg) {
    while (1) {
        const uint64_t now  = (uint64_t)esp_timer_get_time();
        const uint64_t next = s_next_event;
        const uint64_t left = (next > now) ? (next - now) : 0;

        ESP_LOGI(TAG, "state=%s  next_event_in=%llu ms",
                 s_state == FAN_RUNNING ? "RUNNING" : "IDLE   ",
                 left / 1000ULL);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─── WDT task ─────────────────────────────────────────────
static void wdt_task(void* arg) {
    esp_task_wdt_add(NULL);
    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─── app_main ─────────────────────────────────────────────
void app_main(void) {
    ESP_LOGI(TAG, "BOOT");

    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic  = true
    };
    esp_task_wdt_reconfigure(&wdt_cfg);

    gpio_init_all();
    apply_state(FAN_IDLE);

    s_next_event = (uint64_t)esp_timer_get_time()
                   + (PERIOD_US - ON_TIME_US);

    const esp_timer_create_args_t timer_args = {
        .callback              = timer_cb,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_ISR,
        .name                  = "fan_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timer_args, &s_timer);
    esp_timer_start_periodic(s_timer, TIMER_TICK_US);

    ESP_LOGI(TAG, "Timer started — tick every %llu ms",
             TIMER_TICK_US / 1000ULL);

    xTaskCreatePinnedToCore(log_task, "log", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(wdt_task, "wdt", 1024, NULL, 2, NULL, 1);
}