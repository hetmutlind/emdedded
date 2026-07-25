#include <stdio.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/touch_sensor.h"

static const char *TAG = "touch_demo";

struct MyContext {
    std::atomic<int> interrupt_count{0};
    std::atomic<int> last_touch_value{0};
};

static MyContext g_ctx;

static void IRAM_ATTR touch_isr_handler(void *arg) {
    auto *ctx = static_cast<MyContext *>(arg);
    if (ctx == nullptr) {
        return;
    }

    ctx->interrupt_count.fetch_add(1, std::memory_order_relaxed);

    uint32_t raw_value = 0;
    if (touch_pad_read_raw_data(TOUCH_PAD_NUM0, &raw_value) == ESP_OK) {
        ctx->last_touch_value.store(static_cast<int>(raw_value), std::memory_order_relaxed);
    }

    touch_pad_clear_status();
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP-IDF touch interrupt demo");

    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM0);
    touch_pad_set_thresh(TOUCH_PAD_NUM0, 100);
    touch_pad_set_channel_mask(1 << TOUCH_PAD_NUM0);
    touch_pad_fsm_start();
    touch_pad_set_meas_time(0x1000, 0x1000);

    touch_pad_isr_register(touch_isr_handler, &g_ctx, TOUCH_PAD_INTR_MASK_ALL);
    touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ALL);
    touch_pad_clear_status();

    while (true) {
        ESP_LOGI(TAG, "interrupt_count=%d last_touch=%d",
                 g_ctx.interrupt_count.load(std::memory_order_relaxed),
                 g_ctx.last_touch_value.load(std::memory_order_relaxed));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
