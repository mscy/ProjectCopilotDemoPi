#include "lcd.h"
#include "config.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static const char *TAG = "lcd";

#define LCD_HOST            SPI2_HOST
#define LCD_SPI_CLOCK_HZ    (10 * 1000 * 1000)
#define LCD_FILL_STRIP_ROWS 20

static spi_device_handle_t s_spi = NULL;
static bool s_initialized = false;

static esp_err_t lcd_tx(const void *data, size_t len)
{
    if (!s_spi || !data || len == 0) return ESP_OK;
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_polling_transmit(s_spi, &trans);
}

static esp_err_t lcd_write_cmd(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)LCD_DC_GPIO, 0);
    return lcd_tx(&cmd, 1);
}

static esp_err_t lcd_write_data(const uint8_t *data, size_t len)
{
    gpio_set_level((gpio_num_t)LCD_DC_GPIO, 1);
    return lcd_tx(data, len);
}

static esp_err_t lcd_write_u8(uint8_t value)
{
    return lcd_write_data(&value, 1);
}

static esp_err_t lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t data[4];
    esp_err_t err;

    err = lcd_write_cmd(0x2A); // CASET
    if (err != ESP_OK) return err;
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    err = lcd_write_data(data, sizeof(data));
    if (err != ESP_OK) return err;

    err = lcd_write_cmd(0x2B); // RASET
    if (err != ESP_OK) return err;
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    err = lcd_write_data(data, sizeof(data));
    if (err != ESP_OK) return err;

    return lcd_write_cmd(0x2C); // RAMWR
}

esp_err_t lcd_init(void)
{
    if (s_initialized) return ESP_OK;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LCD_DC_GPIO) |
                        (1ULL << LCD_RST_GPIO) |
                        (1ULL << LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio config: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level((gpio_num_t)LCD_DC_GPIO, 0);
    gpio_set_level((gpio_num_t)LCD_BL_GPIO, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_SCLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_FILL_STRIP_ROWS * sizeof(uint16_t) + 64,
    };
    err = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi bus init: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_SPI_CLOCK_HZ,
        .mode = 3,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    err = spi_bus_add_device(LCD_HOST, &devcfg, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi add device: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level((gpio_num_t)LCD_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level((gpio_num_t)LCD_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x01), TAG, "SWRESET");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x11), TAG, "SLPOUT");
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x3A), TAG, "COLMOD");
    ESP_RETURN_ON_ERROR(lcd_write_u8(0x55), TAG, "COLMOD data");

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x36), TAG, "MADCTL");
    ESP_RETURN_ON_ERROR(lcd_write_u8(0x00), TAG, "MADCTL data");

#if LCD_INVERT_COLOR
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x21), TAG, "INVON");
#else
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x20), TAG, "INVOFF");
#endif

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x13), TAG, "NORON");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x29), TAG, "DISPON");
    vTaskDelay(pdMS_TO_TICKS(120));

    gpio_set_level((gpio_num_t)LCD_BL_GPIO, 1);

    s_initialized = true;
    ESP_LOGI(TAG, "ST7789P3 SPI %dx%d ready @ %d Hz", LCD_WIDTH, LCD_HEIGHT, LCD_SPI_CLOCK_HZ);
    return ESP_OK;
}

esp_err_t lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;

    int strip_rows = h < LCD_FILL_STRIP_ROWS ? h : LCD_FILL_STRIP_ROWS;
    size_t strip_pixels = (size_t)w * strip_rows;
    uint16_t *buf = heap_caps_malloc(strip_pixels * sizeof(uint16_t),
                                     MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) {
        ESP_LOGE(TAG, "fill_rect: no DMA memory (%u px)", (unsigned)strip_pixels);
        return ESP_ERR_NO_MEM;
    }

    uint16_t sw = (uint16_t)((color << 8) | (color >> 8));
    for (size_t i = 0; i < strip_pixels; i++) buf[i] = sw;

    esp_err_t err = ESP_OK;
    for (int y0 = 0; y0 < h; y0 += strip_rows) {
        int rows = (h - y0) < strip_rows ? (h - y0) : strip_rows;
        err = lcd_set_window(x, y + y0, x + w - 1, y + y0 + rows - 1);
        if (err != ESP_OK) break;
        gpio_set_level((gpio_num_t)LCD_DC_GPIO, 1);
        err = lcd_tx(buf, (size_t)w * rows * sizeof(uint16_t));
        if (err != ESP_OK) break;
    }

    free(buf);
    return err;
}

esp_err_t lcd_fill(uint16_t color)
{
    return lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

esp_err_t lcd_draw_bitmap(int x, int y, int w, int h, const uint16_t *pixels)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!pixels || w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;
    if (x < 0 || y < 0 || x + w > LCD_WIDTH || y + h > LCD_HEIGHT) return ESP_ERR_INVALID_ARG;

    int strip_rows = h < LCD_FILL_STRIP_ROWS ? h : LCD_FILL_STRIP_ROWS;
    uint16_t *buf = heap_caps_malloc((size_t)w * strip_rows * sizeof(uint16_t),
                                     MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_OK;
    for (int y0 = 0; y0 < h; y0 += strip_rows) {
        int rows = (h - y0) < strip_rows ? (h - y0) : strip_rows;
        for (int i = 0; i < w * rows; i++) {
            uint16_t c = pixels[y0 * w + i];
            buf[i] = (uint16_t)((c << 8) | (c >> 8));
        }

        err = lcd_set_window(x, y + y0, x + w - 1, y + y0 + rows - 1);
        if (err != ESP_OK) break;
        gpio_set_level((gpio_num_t)LCD_DC_GPIO, 1);
        err = lcd_tx(buf, (size_t)w * rows * sizeof(uint16_t));
        if (err != ESP_OK) break;
    }

    free(buf);
    return err;
}

esp_err_t lcd_on(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return lcd_write_cmd(0x29);
}

esp_err_t lcd_off(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return lcd_write_cmd(0x28);
}

esp_err_t lcd_self_test(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "self-test: RGB primaries");
    lcd_fill(LCD_RGB565(255, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(1500));
    lcd_fill(LCD_RGB565(0, 255, 0));
    vTaskDelay(pdMS_TO_TICKS(1500));
    lcd_fill(LCD_RGB565(0, 0, 255));
    vTaskDelay(pdMS_TO_TICKS(1500));
    lcd_fill(LCD_RGB565(20, 20, 20));
    return ESP_OK;
}

void lcd_diagnostic_loop(void)
{
    const struct {
        const char *name;
        uint16_t color;
    } fills[] = {
        { "RED",   LCD_RGB565(255, 0, 0) },
        { "GREEN", LCD_RGB565(0, 255, 0) },
        { "BLUE",  LCD_RGB565(0, 0, 255) },
        { "WHITE", LCD_RGB565(255, 255, 255) },
        { "BLACK", LCD_RGB565(0, 0, 0) },
    };

    ESP_LOGW(TAG, "LCD diagnostic loop enabled");
    while (1) {
        for (size_t i = 0; i < sizeof(fills) / sizeof(fills[0]); i++) {
            ESP_LOGI(TAG, "diag fill: %s", fills[i].name);
            lcd_fill(fills[i].color);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }

        ESP_LOGI(TAG, "diag checkerboard");
        const int cells = 8;
        const int cell = LCD_WIDTH / cells;
        for (int cy = 0; cy < cells; cy++) {
            for (int cx = 0; cx < cells; cx++) {
                uint16_t color = ((cx + cy) & 1) ? LCD_RGB565(255, 255, 255) : LCD_RGB565(0, 0, 0);
                lcd_fill_rect(cx * cell, cy * cell, cell, cell, color);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}