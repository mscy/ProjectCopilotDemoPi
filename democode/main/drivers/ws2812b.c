#include "ws2812b.h"
#include "config.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ws2812";

#define WS_RES_HZ   (10 * 1000 * 1000)   // 10MHz, 100ns per tick
#define T0H_TICKS   3   // 0.3us
#define T0L_TICKS   9   // 0.9us
#define T1H_TICKS   9   // 0.9us
#define T1L_TICKS   3   // 0.3us
#define RST_TICKS   500 // 50us reset

static rmt_channel_handle_t s_chan = NULL;
static rmt_encoder_handle_t s_bytes_enc = NULL;
static rmt_encoder_handle_t s_copy_enc = NULL;
static uint8_t *s_buf = NULL;       // GRB bytes
static uint16_t s_count = 0;

esp_err_t ws2812_init(uint16_t led_count)
{
    if (s_chan) return ESP_OK;
    rmt_tx_channel_config_t cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = WS2812B_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = WS_RES_HZ,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&cfg, &s_chan);
    if (err != ESP_OK) { ESP_LOGE(TAG, "new chan: %s", esp_err_to_name(err)); return err; }

    rmt_bytes_encoder_config_t bcfg = {
        .bit0 = { .level0 = 1, .duration0 = T0H_TICKS, .level1 = 0, .duration1 = T0L_TICKS },
        .bit1 = { .level0 = 1, .duration0 = T1H_TICKS, .level1 = 0, .duration1 = T1L_TICKS },
        .flags.msb_first = 1,
    };
    err = rmt_new_bytes_encoder(&bcfg, &s_bytes_enc);
    if (err != ESP_OK) return err;

    rmt_copy_encoder_config_t ccfg = {};
    err = rmt_new_copy_encoder(&ccfg, &s_copy_enc);
    if (err != ESP_OK) return err;

    s_count = led_count;
    s_buf = calloc(led_count, 3);
    if (!s_buf) return ESP_ERR_NO_MEM;

    rmt_enable(s_chan);
    ESP_LOGI(TAG, "WS2812 ready (gpio=%d count=%u)", WS2812B_GPIO, led_count);
    return ESP_OK;
}

esp_err_t ws2812_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= s_count || !s_buf) return ESP_ERR_INVALID_ARG;
    s_buf[idx*3+0] = g;
    s_buf[idx*3+1] = r;
    s_buf[idx*3+2] = b;
    return ESP_OK;
}

esp_err_t ws2812_refresh(void)
{
    if (!s_chan) return ESP_ERR_INVALID_STATE;
    rmt_transmit_config_t tcfg = { .loop_count = 0 };
    esp_err_t err = rmt_transmit(s_chan, s_bytes_enc, s_buf, s_count * 3, &tcfg);
    if (err != ESP_OK) return err;
    return rmt_tx_wait_all_done(s_chan, 100);
}

esp_err_t ws2812_self_test(void)
{
    if (!s_chan) return ESP_ERR_INVALID_STATE;
    const struct { uint8_t r,g,b; } seq[] = {
        {64,0,0}, {0,64,0}, {0,0,64}, {0,0,0}
    };
    for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); i++) {
        for (uint16_t j = 0; j < s_count; j++)
            ws2812_set(j, seq[i].r, seq[i].g, seq[i].b);
        ws2812_refresh();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    ESP_LOGI(TAG, "self-test PASS");
    return ESP_OK;
}

// =====================================================================
// Second channel: external WS2812 strip on the extension header.
// =====================================================================
static rmt_channel_handle_t s_strip_chan = NULL;
static rmt_encoder_handle_t s_strip_bytes_enc = NULL;
static uint8_t  *s_strip_buf = NULL;
static uint16_t  s_strip_count = 0;
static int       s_strip_gpio = -1;

static void ws2812_strip_deinit(void)
{
    if (s_strip_chan) {
        (void)rmt_disable(s_strip_chan);
        (void)rmt_del_channel(s_strip_chan);
        s_strip_chan = NULL;
    }
    if (s_strip_bytes_enc) {
        (void)rmt_del_encoder(s_strip_bytes_enc);
        s_strip_bytes_enc = NULL;
    }
    free(s_strip_buf);
    s_strip_buf = NULL;
    s_strip_count = 0;
    s_strip_gpio = -1;
}

esp_err_t ws2812_strip_init(int gpio, uint16_t led_count)
{
    if (led_count == 0) return ESP_ERR_INVALID_ARG;

    if (s_strip_chan) {
        if (s_strip_gpio == gpio && s_strip_count == led_count) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "strip reinit gpio=%d->%d count=%u->%u",
                 s_strip_gpio, gpio, (unsigned)s_strip_count, (unsigned)led_count);
        ws2812_strip_deinit();
    }

    rmt_tx_channel_config_t cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 64,
        .resolution_hz = WS_RES_HZ,
        .trans_queue_depth = 4,
    };
    esp_err_t err = rmt_new_tx_channel(&cfg, &s_strip_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip new chan (gpio=%d): %s", gpio, esp_err_to_name(err));
        return err;
    }

    rmt_bytes_encoder_config_t bcfg = {
        .bit0 = { .level0 = 1, .duration0 = T0H_TICKS, .level1 = 0, .duration1 = T0L_TICKS },
        .bit1 = { .level0 = 1, .duration0 = T1H_TICKS, .level1 = 0, .duration1 = T1L_TICKS },
        .flags.msb_first = 1,
    };
    err = rmt_new_bytes_encoder(&bcfg, &s_strip_bytes_enc);
    if (err != ESP_OK) {
        (void)rmt_del_channel(s_strip_chan);
        s_strip_chan = NULL;
        return err;
    }

    s_strip_buf = calloc(led_count, 3);
    if (!s_strip_buf) {
        (void)rmt_del_encoder(s_strip_bytes_enc);
        s_strip_bytes_enc = NULL;
        (void)rmt_del_channel(s_strip_chan);
        s_strip_chan = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_strip_count = led_count;
    s_strip_gpio  = gpio;

    err = rmt_enable(s_strip_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip enable failed: %s", esp_err_to_name(err));
        ws2812_strip_deinit();
        return err;
    }
    ESP_LOGI(TAG, "strip ready (gpio=%d count=%u)", gpio, led_count);
    return ESP_OK;
}

esp_err_t ws2812_strip_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= s_strip_count || !s_strip_buf) return ESP_ERR_INVALID_ARG;
    s_strip_buf[idx*3+0] = g;
    s_strip_buf[idx*3+1] = r;
    s_strip_buf[idx*3+2] = b;
    return ESP_OK;
}

esp_err_t ws2812_strip_fill(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_strip_buf) return ESP_ERR_INVALID_STATE;
    for (uint16_t i = 0; i < s_strip_count; i++) {
        s_strip_buf[i*3+0] = g;
        s_strip_buf[i*3+1] = r;
        s_strip_buf[i*3+2] = b;
    }
    return ESP_OK;
}

esp_err_t ws2812_strip_refresh(void)
{
    if (!s_strip_chan) return ESP_ERR_INVALID_STATE;
    rmt_transmit_config_t tcfg = { .loop_count = 0 };
    esp_err_t err = rmt_transmit(s_strip_chan, s_strip_bytes_enc,
                                 s_strip_buf, s_strip_count * 3, &tcfg);
    if (err != ESP_OK) return err;
    return rmt_tx_wait_all_done(s_strip_chan, 100);
}

uint16_t ws2812_strip_count(void)
{
    return s_strip_count;
}

static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t region = (h / 60) % 6;
    uint16_t f = (h % 60) * 255 / 60;
    uint8_t  p = (uint16_t)v * (255 - s) / 255;
    uint8_t  q = (uint16_t)v * (255 - ((uint16_t)s * f / 255)) / 255;
    uint8_t  t = (uint16_t)v * (255 - ((uint16_t)s * (255 - f) / 255)) / 255;
    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default:*r = v; *g = p; *b = q; break;
    }
}

esp_err_t ws2812_strip_rainbow_sweep(uint32_t cycles, uint32_t step_ms)
{
    if (!s_strip_chan) return ESP_ERR_INVALID_STATE;
    const uint16_t n = s_strip_count;
    for (uint32_t c = 0; c < cycles; c++) {
        for (uint16_t offset = 0; offset < 360; offset += 6) {
            for (uint16_t i = 0; i < n; i++) {
                uint16_t h = (offset + i * (360 / (n ? n : 1))) % 360;
                uint8_t r, g, b;
                hsv_to_rgb(h, 255, 140, &r, &g, &b);
                ws2812_strip_set(i, r, g, b);
            }
            ws2812_strip_refresh();
            vTaskDelay(pdMS_TO_TICKS(step_ms));
        }
    }
    ws2812_strip_fill(0, 0, 0);
    ws2812_strip_refresh();
    return ESP_OK;
}

