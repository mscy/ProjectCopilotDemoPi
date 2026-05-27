#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t ws2812_init(uint16_t led_count);
esp_err_t ws2812_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
esp_err_t ws2812_refresh(void);
esp_err_t ws2812_self_test(void);

// Second WS2812 channel for the external strip on the extension header.
esp_err_t ws2812_strip_init(int gpio, uint16_t led_count);
esp_err_t ws2812_strip_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
esp_err_t ws2812_strip_fill(uint8_t r, uint8_t g, uint8_t b);
esp_err_t ws2812_strip_refresh(void);
esp_err_t ws2812_strip_rainbow_sweep(uint32_t cycles, uint32_t step_ms);
uint16_t  ws2812_strip_count(void);
