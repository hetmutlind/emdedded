#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#define ADC_CHANNEL      ADC_CHANNEL_8
#define ADC_UNIT         ADC_UNIT_1
#define ADC_ATTEN        ADC_ATTEN_DB_12
#define ADC_BITWIDTH     ADC_BITWIDTH_12

#define SERVO_PIN        14
#define SERVO_FREQ       50
#define SERVO_PERIOD_US  20000u
#define SERVO_RESOLUTION LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY   ((1u << SERVO_RESOLUTION) - 1u)
#define SERVO_UNIT       LEDC_TIMER_0
#define SERVO_CHANNEL    LEDC_CHANNEL_0

#define SUPERLOOP_DELAY_MS 100

#define LIGHT_MIN_MV 50
#define LIGHT_MAX_MV 3000

#define SERVO_MIN_DEG 10
#define SERVO_MAX_DEG 170

#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500

static const char *TAG = "mini_project";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

static int clamp_value(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int map_value(int value, int in_min, int in_max, int out_min, int out_max)
{
    if (in_min == in_max) {
        return out_max;
    }

    value = clamp_value(value, in_min, in_max);
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

static esp_err_t ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_new_unit(&unit_config, &adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &channel_config));

    adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = ADC_UNIT,
        .chan = ADC_CHANNEL,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_cali_create_scheme_curve_fitting(&calibration_config, &cali_handle));

    return ESP_OK;
}

static esp_err_t servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = SERVO_UNIT,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz = SERVO_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_CHANNEL,
        .timer_sel = SERVO_UNIT,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = SERVO_PIN,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&channel_config));

    return ESP_OK;
}

static void set_servo_angle_from_light_mv(int light_mv)
{
    int angle_deg = map_value(light_mv, LIGHT_MIN_MV, LIGHT_MAX_MV, SERVO_MIN_DEG, SERVO_MAX_DEG);
    int pulse_us = map_value(angle_deg, SERVO_MIN_DEG, SERVO_MAX_DEG, SERVO_MIN_US, SERVO_MAX_US);
    uint32_t duty = (uint32_t)((SERVO_MAX_DUTY * pulse_us) / SERVO_PERIOD_US);

    ESP_LOGI(TAG, "light_mv=%d angle_deg=%d pulse_us=%d duty=%lu", light_mv, angle_deg, pulse_us, duty);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));
}

static int read_light_mv(void)
{
    int raw_value = 0;
    int calibrated_mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_value, &calibrated_mv));

    return calibrated_mv;
}

void app_main(void)
{
    ESP_ERROR_CHECK(ldr_init());
    ESP_ERROR_CHECK(servo_init());

    while (true) {
        int light_mv = read_light_mv();
        ESP_LOGI(TAG, "adc_raw=%d voltage_mv=%d", 0, light_mv);
        set_servo_angle_from_light_mv(light_mv);
        vTaskDelay(pdMS_TO_TICKS(SUPERLOOP_DELAY_MS));
    }
}
