#pragma once
#include "esp_err.h"

esp_err_t led_pwm_init(void);
esp_err_t led_pwm_set_rgb(uint8_t r, uint8_t g, uint8_t b); // 0..255
esp_err_t lcd_backlight_set(uint8_t pct);                   // 0..100
