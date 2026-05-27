#include "m5unit_lcd_i2c.h"

#include <math.h>
#include <string.h>
#include "config.h"
#include "esp_log.h"

static const char *TAG = "m5lcd_i2c";

// M5 Unit LCD (ST7789V2 + bridge MCU) command set.
#define CMD_READ_ID       0x04
#define CMD_BRIGHTNESS    0x22
#define CMD_ROTATE        0x36
#define CMD_FILLRECT_24   0x6B

static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_addr = 0;
static uint8_t s_id[4] = {0};
static bool s_ready = false;
static bool s_acc_smooth_init = false;
static float s_ax_smooth = 0.0f;
static float s_ay_smooth = 0.0f;
static float s_az_smooth = 0.0f;
static float s_g_smooth = 1.0f;
static bool s_acc_ui_init = false;

static esp_err_t m5unit_lcd_tx(const uint8_t *data, size_t len)
{
    if (!s_dev || !data || len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit(s_dev, data, len, 30);
}

static esp_err_t m5unit_lcd_txrx(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    if (!s_dev || !tx || tx_len == 0 || !rx || rx_len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive(s_dev, tx, tx_len, rx, rx_len, 30);
}

esp_err_t m5unit_lcd_i2c_init(i2c_master_bus_handle_t bus, uint8_t i2c_addr)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    if (s_dev && s_addr == i2c_addr && s_ready) {
        return ESP_OK;
    }

    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        s_ready = false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = I2C_FREQ_HZ,
        .scl_wait_us = 0,
        .flags.disable_ack_check = 0,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add dev 0x%02X failed: %s", i2c_addr, esp_err_to_name(err));
        return err;
    }

    s_addr = i2c_addr;

    const uint8_t cmd = CMD_READ_ID;
    err = m5unit_lcd_txrx(&cmd, 1, s_id, sizeof(s_id));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read id @0x%02X failed: %s", s_addr, esp_err_to_name(err));
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return err;
    }

    const uint8_t rotate_cmd[2] = { CMD_ROTATE, 0 };
    err = m5unit_lcd_tx(rotate_cmd, sizeof(rotate_cmd));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set rotate failed: %s", esp_err_to_name(err));
    }

    const uint8_t br_cmd[2] = { CMD_BRIGHTNESS, 180 };
    err = m5unit_lcd_tx(br_cmd, sizeof(br_cmd));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set brightness failed: %s", esp_err_to_name(err));
    }

    s_ready = true;
    ESP_LOGI(TAG, "M5 I2C LCD ready @0x%02X id=%02X %02X %02X %02X",
             s_addr, s_id[0], s_id[1], s_id[2], s_id[3]);
    return ESP_OK;
}

bool m5unit_lcd_i2c_is_ready(void)
{
    return s_ready;
}

uint8_t m5unit_lcd_i2c_addr(void)
{
    return s_addr;
}

void m5unit_lcd_i2c_get_id(uint8_t out_id[4])
{
    if (!out_id) return;
    memcpy(out_id, s_id, sizeof(s_id));
}

esp_err_t m5unit_lcd_i2c_set_brightness(uint8_t brightness)
{
    const uint8_t cmd[2] = { CMD_BRIGHTNESS, brightness };
    return m5unit_lcd_tx(cmd, sizeof(cmd));
}

esp_err_t m5unit_lcd_i2c_fill_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return m5unit_lcd_i2c_fill_rect_rgb(0, 0,
                                        (uint8_t)(LCD_WIDTH - 1),
                                        (uint8_t)(LCD_HEIGHT - 1),
                                        r, g, b);
}

esp_err_t m5unit_lcd_i2c_fill_rect_rgb(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye,
                                       uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t cmd[8] = {
        CMD_FILLRECT_24,
        xs,
        ys,
        xe,
        ye,
        r,
        g,
        b,
    };
    return m5unit_lcd_tx(cmd, sizeof(cmd));
}

static float clamp_g(float g)
{
    if (g > 2.0f) return 2.0f;
    if (g < -2.0f) return -2.0f;
    return g;
}

static esp_err_t draw_hseg(uint8_t x, uint8_t y, uint8_t w, uint8_t t,
                           uint8_t r, uint8_t g, uint8_t b)
{
    return m5unit_lcd_i2c_fill_rect_rgb(x, y, (uint8_t)(x + w - 1), (uint8_t)(y + t - 1), r, g, b);
}

static esp_err_t draw_vseg(uint8_t x, uint8_t y, uint8_t h, uint8_t t,
                           uint8_t r, uint8_t g, uint8_t b)
{
    return m5unit_lcd_i2c_fill_rect_rgb(x, y, (uint8_t)(x + t - 1), (uint8_t)(y + h - 1), r, g, b);
}

static esp_err_t draw_digit_7seg(uint8_t x, uint8_t y, uint8_t digit,
                                 uint8_t r, uint8_t g, uint8_t b)
{
    // 7-seg layout: a,b,c,d,e,f,g
    static const uint8_t mask[] = {
        0x3F, // 0
        0x06, // 1
        0x5B, // 2
        0x4F, // 3
        0x66, // 4
        0x6D, // 5
        0x7D, // 6
        0x07, // 7
        0x7F, // 8
        0x6F, // 9
    };
    if (digit > 9) return ESP_ERR_INVALID_ARG;

    const uint8_t m = mask[digit];
    const uint8_t w = 7;
    const uint8_t h = 11;
    const uint8_t t = 1;
    esp_err_t err;

    if (m & 0x01) { err = draw_hseg((uint8_t)(x + t), y, w, t, r, g, b); if (err != ESP_OK) return err; }           // a
    if (m & 0x02) { err = draw_vseg((uint8_t)(x + w + t), (uint8_t)(y + t), h, t, r, g, b); if (err != ESP_OK) return err; } // b
    if (m & 0x04) { err = draw_vseg((uint8_t)(x + w + t), (uint8_t)(y + h + 2 * t), h, t, r, g, b); if (err != ESP_OK) return err; } // c
    if (m & 0x08) { err = draw_hseg((uint8_t)(x + t), (uint8_t)(y + 2 * h + 2 * t), w, t, r, g, b); if (err != ESP_OK) return err; } // d
    if (m & 0x10) { err = draw_vseg(x, (uint8_t)(y + h + 2 * t), h, t, r, g, b); if (err != ESP_OK) return err; }     // e
    if (m & 0x20) { err = draw_vseg(x, (uint8_t)(y + t), h, t, r, g, b); if (err != ESP_OK) return err; }             // f
    if (m & 0x40) { err = draw_hseg((uint8_t)(x + t), (uint8_t)(y + h + t), w, t, r, g, b); if (err != ESP_OK) return err; } // g

    return ESP_OK;
}

static esp_err_t draw_g_value(float g_val)
{
    // Clamp shown value to 0.00 .. 9.99g
    if (g_val < 0.0f) g_val = 0.0f;
    if (g_val > 9.99f) g_val = 9.99f;
    int scaled = (int)lroundf(g_val * 100.0f);
    int d0 = scaled / 100;
    int d1 = (scaled / 10) % 10;
    int d2 = scaled % 10;

    // Clear G-value band (240x135 layout).
    esp_err_t err = m5unit_lcd_i2c_fill_rect_rgb(14, 102, 94, 124, 6, 8, 14);
    if (err != ESP_OK) return err;

    // Left marker: draw a clear uppercase 'G' glyph.
    err = m5unit_lcd_i2c_fill_rect_rgb(14, 104, 20, 116, 255, 220, 90);
    if (err != ESP_OK) return err;
    err = m5unit_lcd_i2c_fill_rect_rgb(16, 106, 18, 113, 6, 8, 14);
    if (err != ESP_OK) return err;
    // Right-side opening + inner horizontal notch to distinguish 'G' from 'O'/'F'.
    err = m5unit_lcd_i2c_fill_rect_rgb(18, 106, 20, 109, 6, 8, 14);
    if (err != ESP_OK) return err;
    err = m5unit_lcd_i2c_fill_rect_rgb(17, 110, 20, 111, 255, 220, 90);
    if (err != ESP_OK) return err;
    err = m5unit_lcd_i2c_fill_rect_rgb(18, 110, 20, 113, 255, 220, 90);
    if (err != ESP_OK) return err;

    const uint8_t cr = 255, cg = 255, cb = 255;
    err = draw_digit_7seg(24, 106, (uint8_t)d0, cr, cg, cb);
    if (err != ESP_OK) return err;
    err = draw_digit_7seg(36, 106, (uint8_t)d1, cr, cg, cb);
    if (err != ESP_OK) return err;
    err = draw_digit_7seg(48, 106, (uint8_t)d2, cr, cg, cb);
    if (err != ESP_OK) return err;

    // Decimal point between d0 and d1.
    err = m5unit_lcd_i2c_fill_rect_rgb(34, 117, 35, 118, 255, 255, 255);
    return err;
}

static esp_err_t draw_accel_bar(uint8_t y0, float g,
                                uint8_t r, uint8_t gch, uint8_t b)
{
    const uint8_t x_mid = (uint8_t)(LCD_WIDTH / 2);
    const uint8_t x_max = (uint8_t)(LCD_WIDTH - 6);
    const uint8_t x_min = 6;
    const uint8_t half_span = (uint8_t)((x_max - x_mid) > (x_mid - x_min)
                               ? (x_mid - x_min) : (x_max - x_mid));
    const uint8_t bar_h = 28;

    g = clamp_g(g);
    int len = (int)lroundf(fabsf(g) * (float)half_span / 2.0f);

    // Clear row first.
    esp_err_t err = m5unit_lcd_i2c_fill_rect_rgb(x_min, y0, x_max, (uint8_t)(y0 + bar_h), 12, 14, 24);
    if (err != ESP_OK) return err;

    // Center marker.
    err = m5unit_lcd_i2c_fill_rect_rgb((uint8_t)(x_mid - 1), y0, (uint8_t)(x_mid + 1), (uint8_t)(y0 + bar_h), 70, 70, 70);
    if (err != ESP_OK) return err;

    if (len <= 0) {
        return ESP_OK;
    }

    if (g >= 0.0f) {
        uint8_t xe = (uint8_t)(x_mid + len);
        if (xe > x_max) xe = x_max;
        return m5unit_lcd_i2c_fill_rect_rgb(x_mid, (uint8_t)(y0 + 3), xe, (uint8_t)(y0 + bar_h - 3), r, gch, b);
    }

    uint8_t xs = (uint8_t)(x_mid - len);
    if (xs < x_min) xs = x_min;
    return m5unit_lcd_i2c_fill_rect_rgb(xs, (uint8_t)(y0 + 3), x_mid, (uint8_t)(y0 + bar_h - 3), r, gch, b);
}

esp_err_t m5unit_lcd_i2c_show_accel(float ax_g, float ay_g, float az_g, uint16_t frame)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    const float g_raw = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

    // Exponential moving average for smoother visual refresh.
    // Higher alpha follows motion faster; lower alpha is smoother.
    const float alpha_axis = 0.20f;
    const float alpha_g = 0.15f;
    if (!s_acc_smooth_init) {
        s_ax_smooth = ax_g;
        s_ay_smooth = ay_g;
        s_az_smooth = az_g;
        s_g_smooth = g_raw;
        s_acc_smooth_init = true;
    } else {
        s_ax_smooth += alpha_axis * (ax_g - s_ax_smooth);
        s_ay_smooth += alpha_axis * (ay_g - s_ay_smooth);
        s_az_smooth += alpha_axis * (az_g - s_az_smooth);
        s_g_smooth += alpha_g * (g_raw - s_g_smooth);
    }

    esp_err_t err;
    if (!s_acc_ui_init) {
        err = m5unit_lcd_i2c_fill_rgb(4, 6, 12);
        if (err != ESP_OK) return err;
        s_acc_ui_init = true;
    }

    err = draw_accel_bar(6, s_ax_smooth, 255, 70, 70);    // X axis
    if (err != ESP_OK) return err;
    err = draw_accel_bar(38, s_ay_smooth, 80, 255, 120);  // Y axis
    if (err != ESP_OK) return err;
    err = draw_accel_bar(70, s_az_smooth, 80, 150, 255);  // Z axis
    if (err != ESP_OK) return err;

    err = draw_g_value(s_g_smooth);
    if (err != ESP_OK) return err;

    // Small heartbeat dot to confirm refresh.
    uint8_t x = (uint8_t)(8 + (frame % (LCD_WIDTH - 16)));
    err = m5unit_lcd_i2c_fill_rect_rgb(0, 2, (uint8_t)(LCD_WIDTH - 1), 6, 4, 6, 12);
    if (err != ESP_OK) return err;
    return m5unit_lcd_i2c_fill_rect_rgb(x, 2, (uint8_t)(x + 4), 6, 255, 255, 255);
}

esp_err_t m5unit_lcd_i2c_test_step(uint16_t frame)
{
    static const uint8_t colors[][3] = {
        {255,   0,   0},
        {255, 128,   0},
        {255, 255,   0},
        {  0, 255,   0},
        {  0, 180, 255},
        {  0,   0, 255},
        {180,   0, 255},
        {255, 255, 255},
    };
    const uint16_t idx = (frame / 6U) % (sizeof(colors) / sizeof(colors[0]));
    return m5unit_lcd_i2c_fill_rgb(colors[idx][0], colors[idx][1], colors[idx][2]);
}
