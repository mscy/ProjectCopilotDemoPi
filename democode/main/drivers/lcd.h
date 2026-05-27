#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t lcd_init(void);
esp_err_t lcd_self_test(void);
void lcd_diagnostic_loop(void);
esp_err_t lcd_fill(uint16_t color);
esp_err_t lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
esp_err_t lcd_draw_bitmap(int x, int y, int w, int h, const uint16_t *pixels);
esp_err_t lcd_on(void);
esp_err_t lcd_off(void);

// Helpers
#define LCD_RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))
