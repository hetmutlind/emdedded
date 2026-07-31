#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "u8g2.h"

static const char *TAG = "smart_clock";

// ========== Pin Definitions (ESP32-S3) ==========
#define I2C_MASTER_SCL_IO   9
#define I2C_MASTER_SDA_IO   8
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  400000

#define DISPLAY_SCLK        12
#define DISPLAY_MOSI        11
#define DISPLAY_CS          10
#define DISPLAY_DC          13
#define DISPLAY_RESET       14

#define LED_STATUS          19   // Червоний
#define LED_BACKLIGHT       18   // Синій

#define BTN_MENU            37
#define BTN_SET             38
#define ENC_A               35
#define ENC_B               36
#define ENC_SW              39

#define DS1307_ADDR         0x68
#define BME280_ADDR         0x76

// ========== SPI handle ==========
static spi_device_handle_t spi;

// ========== Data Structures ==========
struct RtcTime {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

struct Bme280CalibData {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
};

struct Bme280State {
    Bme280CalibData calib;
    int32_t t_fine;
    bool initialized;
};

// ========== Global Variables ==========
static Bme280State g_bme280;
static u8g2_t u8g2;

static bool g_led_state = false;
static int32_t g_encoder_pos = 0;
static uint8_t g_menu_mode = 0;           // 0=RUN, 1=SET HOURS, 2=SET MINUTES, 3=SET DAY, 4=SET MONTH, 5=SET YEAR
static bool g_editing = false;
static bool g_prev_enc_sw = true;
static bool g_prev_menu = true;
static bool g_prev_set = true;
static RtcTime g_time = {12, 0, 0, 1, 1, 26};

// ========== BCD Helpers ==========
static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}
static uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

// ========== I2C Low‑Level ==========
static esp_err_t i2c_write_reg(i2c_port_t port, uint8_t slave_addr, uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slave_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg(i2c_port_t port, uint8_t slave_addr, uint8_t reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slave_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slave_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// ========== DS1307 RTC ==========
static esp_err_t ds1307_read_time(RtcTime *time) {
    uint8_t buf[7] = {0};
    esp_err_t err = i2c_read_reg(I2C_MASTER_NUM, DS1307_ADDR, 0x00, buf, 7);
    if (err != ESP_OK) return err;
    time->second = bcd2dec(buf[0] & 0x7F);
    time->minute = bcd2dec(buf[1]);
    time->hour   = bcd2dec(buf[2] & 0x3F);
    time->day    = bcd2dec(buf[3]);
    time->month  = bcd2dec(buf[4]);
    time->year   = bcd2dec(buf[6]);
    return ESP_OK;
}

static esp_err_t ds1307_set_time(const RtcTime *time) {
    uint8_t buf[7] = {0};
    buf[0] = dec2bcd(time->second);
    buf[1] = dec2bcd(time->minute);
    buf[2] = dec2bcd(time->hour);
    buf[3] = dec2bcd(time->day);
    buf[4] = dec2bcd(time->month);
    buf[6] = dec2bcd(time->year);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write(cmd, buf, 7, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// ========== BME280 Sensor ==========
static void bme280_read_calib(Bme280State *state) {
    uint8_t buf[24] = {0};
    i2c_read_reg(I2C_MASTER_NUM, BME280_ADDR, 0x88, buf, 24);
    state->calib.dig_T1 = (uint16_t)(buf[0] | (buf[1] << 8));
    state->calib.dig_T2 = (int16_t)(buf[2] | (buf[3] << 8));
    state->calib.dig_T3 = (int16_t)(buf[4] | (buf[5] << 8));
    state->calib.dig_P1 = (uint16_t)(buf[6] | (buf[7] << 8));
    state->calib.dig_P2 = (int16_t)(buf[8] | (buf[9] << 8));
    state->calib.dig_P3 = (int16_t)(buf[10] | (buf[11] << 8));
    state->calib.dig_P4 = (int16_t)(buf[12] | (buf[13] << 8));
    state->calib.dig_P5 = (int16_t)(buf[14] | (buf[15] << 8));
    state->calib.dig_P6 = (int16_t)(buf[16] | (buf[17] << 8));
    state->calib.dig_P7 = (int16_t)(buf[18] | (buf[19] << 8));
    state->calib.dig_P8 = (int16_t)(buf[20] | (buf[21] << 8));
    state->calib.dig_P9 = (int16_t)(buf[22] | (buf[23] << 8));
    uint8_t h1 = 0;
    i2c_read_reg(I2C_MASTER_NUM, BME280_ADDR, 0xA1, &h1, 1);
    state->calib.dig_H1 = h1;
    uint8_t hbuf[8] = {0};
    i2c_read_reg(I2C_MASTER_NUM, BME280_ADDR, 0xE1, hbuf, 8);
    state->calib.dig_H2 = (int16_t)(hbuf[0] | (hbuf[1] << 8));
    state->calib.dig_H3 = hbuf[2];
    state->calib.dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    state->calib.dig_H5 = (int16_t)((hbuf[5] << 4) | ((hbuf[4] >> 4) & 0x0F));
    state->calib.dig_H6 = (int8_t)hbuf[6];
}

static bool bme280_init(Bme280State *state) {
    uint8_t chip_id = 0;
    if (i2c_read_reg(I2C_MASTER_NUM, BME280_ADDR, 0xD0, &chip_id, 1) != ESP_OK) return false;
    if (chip_id != 0x60) {
        ESP_LOGE(TAG, "BME280 chip id mismatch: 0x%02X", chip_id);
        return false;
    }
    bme280_read_calib(state);
    i2c_write_reg(I2C_MASTER_NUM, BME280_ADDR, 0xF2, 0x01);
    i2c_write_reg(I2C_MASTER_NUM, BME280_ADDR, 0xF4, 0x27);
    i2c_write_reg(I2C_MASTER_NUM, BME280_ADDR, 0xF5, 0x00);
    state->initialized = true;
    return true;
}

static int32_t bme280_compensate_temp(int32_t adc_T, Bme280State *state) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)state->calib.dig_T1 << 1))) * (int32_t)state->calib.dig_T2) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)state->calib.dig_T1) * ((adc_T >> 4) - (int32_t)state->calib.dig_T1)) >> 12) * (int32_t)state->calib.dig_T3) >> 14;
    state->t_fine = var1 + var2;
    return (state->t_fine * 5 + 128) >> 8;
}

static uint32_t bme280_compensate_humidity(int32_t adc_H, const Bme280State *state) {
    int32_t v_x1_u32r = (state->t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)state->calib.dig_H4) << 20) - (((int32_t)state->calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15)
                * (((((((v_x1_u32r * (int32_t)state->calib.dig_H6) >> 10) * (((v_x1_u32r * (int32_t)state->calib.dig_H3) >> 11) + ((int32_t)32768))) >> 10)
                    + ((int32_t)2097152)) * (int32_t)state->calib.dig_H2 + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)state->calib.dig_H1) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (uint32_t)((v_x1_u32r >> 12) / 1024);
}

static bool bme280_read_values(float *temperature_c, float *humidity_pct) {
    uint8_t data[8] = {0};
    if (i2c_read_reg(I2C_MASTER_NUM, BME280_ADDR, 0xF7, data, 8) != ESP_OK) return false;
    int32_t adc_p = (int32_t)((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
    int32_t adc_t = (int32_t)((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
    int32_t adc_h = (int32_t)((uint16_t)data[6] << 8) | data[7];
    (void)adc_p;
    *temperature_c = (float)bme280_compensate_temp(adc_t, &g_bme280) / 100.0f;
    *humidity_pct = (float)bme280_compensate_humidity(adc_h, &g_bme280) / 1024.0f;
    return true;
}

// ========== GPIO, I2C, SPI Setup ==========
static void gpio_setup(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BTN_MENU) | (1ULL << BTN_SET) |
                           (1ULL << ENC_A) | (1ULL << ENC_B) | (1ULL << ENC_SW);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    gpio_set_direction((gpio_num_t)LED_STATUS, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LED_BACKLIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LED_STATUS, 0);
    gpio_set_level((gpio_num_t)LED_BACKLIGHT, 0);
}

static void i2c_master_init(void) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

// ========== Custom SPI functions for u8g2 ==========
static uint8_t u8x8_byte_esp32_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    uint8_t *data = (uint8_t *)arg_ptr;
    switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
            for (size_t i = 0; i < arg_int; i++) {
                spi_transaction_t t;
                memset(&t, 0, sizeof(t));
                t.length = 8;
                t.tx_buffer = &data[i];
                esp_err_t ret = spi_device_transmit(spi, &t);
                if (ret != ESP_OK) return 0;
            }
            break;
        }
        case U8X8_MSG_BYTE_INIT:
            break;
        case U8X8_MSG_BYTE_SET_DC:
            gpio_set_level((gpio_num_t)DISPLAY_DC, arg_int);
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            gpio_set_level((gpio_num_t)DISPLAY_CS, 0);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            gpio_set_level((gpio_num_t)DISPLAY_CS, 1);
            break;
        default:
            return 0;
    }
    return 1;
}

static uint8_t u8x8_gpio_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;
        case U8X8_MSG_GPIO_RESET:
            gpio_set_level((gpio_num_t)DISPLAY_RESET, arg_int);
            break;
        case U8X8_MSG_GPIO_CS:
            gpio_set_level((gpio_num_t)DISPLAY_CS, arg_int);
            break;
        case U8X8_MSG_GPIO_DC:
            gpio_set_level((gpio_num_t)DISPLAY_DC, arg_int);
            break;
        default:
            return 0;
    }
    return 1;
}

static void spi_display_init(void) {
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num = DISPLAY_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = DISPLAY_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = 2 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 7;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));

    gpio_set_direction((gpio_num_t)DISPLAY_CS, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)DISPLAY_CS, 1);
    gpio_set_direction((gpio_num_t)DISPLAY_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)DISPLAY_RESET, GPIO_MODE_OUTPUT);

    gpio_set_level((gpio_num_t)DISPLAY_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)DISPLAY_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

// ========== Encoder Reading (виправлено напрямок) ==========
static void read_encoder() {
    static bool last_a = false;
    static uint32_t last_time = 0;
    uint32_t now = esp_timer_get_time(); // мікросекунди

    bool a = gpio_get_level((gpio_num_t)ENC_A) == 0;
    bool b = gpio_get_level((gpio_num_t)ENC_B) == 0;

    // Якщо змінився стан A (основний сигнал)
    if (a != last_a) {
        // Ігноруємо занадто швидкі зміни (дребезг) – мінімум 1 мс
        if ((now - last_time) > 1000) { // 1 мс
            // Визначаємо напрямок: якщо A == B, то один напрямок, інакше інший
            if (a == b) {
                g_encoder_pos++;
            } else {
                g_encoder_pos--;
            }
            last_time = now;
        }
        last_a = a;
    }
}

// ========== Input Processing ==========
static void process_inputs(RtcTime *time) {
    bool menu = gpio_get_level((gpio_num_t)BTN_MENU) == 0;
    bool set  = gpio_get_level((gpio_num_t)BTN_SET) == 0;
    bool enc_sw = gpio_get_level((gpio_num_t)ENC_SW) == 0;

    // Перемикання режимів
    if (!g_prev_menu && menu) {
        uint8_t new_mode = (g_menu_mode + 1) % 6;
        if (g_menu_mode != 0 && new_mode == 0) {
            if (ds1307_set_time(&g_time) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set RTC time");
            }
            g_editing = false;
        } else if (new_mode != 0 && !g_editing) {
            RtcTime current_time;
            if (ds1307_read_time(&current_time) == ESP_OK) {
                g_time = current_time;
            }
            g_editing = true;
        }
        g_menu_mode = new_mode;
    }

    // Кнопка SET – перемикання додаткового світлодіода (не використовується для температури)
    if (!g_prev_set && set) {
        g_led_state = !g_led_state;
    }

    if (!g_prev_enc_sw && enc_sw) {
        g_encoder_pos = 0;
    }

    g_prev_menu = menu;
    g_prev_set = set;
    g_prev_enc_sw = enc_sw;

    // Зміна параметрів з кроком 1 (для всіх)
    if (g_menu_mode == 1) { // SET HOURS
        if (g_encoder_pos > 0) {
            time->hour = (time->hour + 1) % 24;
            g_encoder_pos = 0;
        } else if (g_encoder_pos < 0) {
            time->hour = (time->hour + 23) % 24;
            g_encoder_pos = 0;
        }
    } else if (g_menu_mode == 2) { // SET MINUTES
        if (g_encoder_pos > 0) {
            time->minute = (time->minute + 1) % 60;
            g_encoder_pos = 0;
        } else if (g_encoder_pos < 0) {
            time->minute = (time->minute + 59) % 60;
            g_encoder_pos = 0;
        }
    } else if (g_menu_mode == 3) { // SET DAY – крок 1
        if (g_encoder_pos > 0) {
            if (time->day < 31) time->day++;
            else time->day = 1;
            g_encoder_pos = 0;
        } else if (g_encoder_pos < 0) {
            if (time->day > 1) time->day--;
            else time->day = 31;
            g_encoder_pos = 0;
        }
    } else if (g_menu_mode == 4) { // SET MONTH – крок 1
        if (g_encoder_pos > 0) {
            time->month = (time->month % 12) + 1;
            g_encoder_pos = 0;
        } else if (g_encoder_pos < 0) {
            time->month = (time->month + 11) % 12 + 1;
            g_encoder_pos = 0;
        }
    } else if (g_menu_mode == 5) { // SET YEAR – крок 1
        if (g_encoder_pos > 0) {
            if (time->year < 99) time->year++;
            else time->year = 0;
            g_encoder_pos = 0;
        } else if (g_encoder_pos < 0) {
            if (time->year > 0) time->year--;
            else time->year = 99;
            g_encoder_pos = 0;
        }
    }
}

// ========== Display Drawing ==========
static void draw_screen(const RtcTime &time, float temperature_c, float humidity_pct) {
    char buf[32];
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 0, 8, "Smart Clock");

    const char *mode_str;
    switch (g_menu_mode) {
        case 1: mode_str = "SET H"; break;
        case 2: mode_str = "SET M"; break;
        case 3: mode_str = "SET D"; break;
        case 4: mode_str = "SET MO"; break;
        case 5: mode_str = "SET Y"; break;
        default: mode_str = "RUN"; break;
    }
    u8g2_DrawStr(&u8g2, 90, 8, mode_str);

    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", time.hour, time.minute, time.second);
    u8g2_SetFont(&u8g2, u8g2_font_logisoso20_tf);
    u8g2_DrawStr(&u8g2, 8, 30, buf);

    u8g2_SetFont(&u8g2, u8g2_font_5x8_tf);
    snprintf(buf, sizeof(buf), "%02u/%02u/20%02u  T:%.1fC  H:%.1f%%",
             time.day, time.month, time.year, temperature_c, humidity_pct);
    u8g2_DrawStr(&u8g2, 2, 56, buf);

    u8g2_DrawBox(&u8g2, 2, 2, 4, 4);
    if (g_led_state) {
        u8g2_DrawFrame(&u8g2, 108, 2, 12, 6);
    }

    u8g2_SendBuffer(&u8g2);
}

// ========== Оновлення світлодіодів за температурою ==========
static void update_leds_by_temperature(float temp) {
    if (temp >= 28.0f) {
        gpio_set_level((gpio_num_t)LED_STATUS, 1);   // червоний
        gpio_set_level((gpio_num_t)LED_BACKLIGHT, 0); // синій вимкнено
    } else {
        gpio_set_level((gpio_num_t)LED_STATUS, 0);   // червоний вимкнено
        gpio_set_level((gpio_num_t)LED_BACKLIGHT, 1); // синій
    }
}

// ========== Main Application ==========
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Starting smart clock firmware");

    gpio_setup();
    i2c_master_init();
    spi_display_init();

    if (!bme280_init(&g_bme280)) {
        ESP_LOGW(TAG, "BME280 not detected, using placeholder values");
    }

    if (ds1307_set_time(&g_time) != ESP_OK) {
        ESP_LOGW(TAG, "RTC init failed, continuing with static time");
    }

    u8g2_Setup_sh1106_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8x8_byte_esp32_hw_spi,
        u8x8_gpio_esp32
    );
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFontRefHeightExtendedText(&u8g2);
    u8g2_SetFontPosTop(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);

    while (true) {
        if (!g_editing) {
            RtcTime current_time;
            if (ds1307_read_time(&current_time) == ESP_OK) {
                g_time = current_time;
            }
        }

        float temperature_c = 25.0f, humidity_pct = 45.0f;
        if (g_bme280.initialized) {
            if (!bme280_read_values(&temperature_c, &humidity_pct)) {
                ESP_LOGW(TAG, "BME280 read failed");
            }
        }

        update_leds_by_temperature(temperature_c);

        read_encoder();             // викликаємо функцію
        process_inputs(&g_time);
        draw_screen(g_time, temperature_c, humidity_pct);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}