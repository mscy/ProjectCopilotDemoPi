#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t m5unit_lcd_i2c_init(i2c_master_bus_handle_t bus, uint8_t i2c_addr);
bool      m5unit_lcd_i2c_is_ready(void);
uint8_t   m5unit_lcd_i2c_addr(void);
void      m5unit_lcd_i2c_get_id(uint8_t out_id[4]);
esp_err_t m5unit_lcd_i2c_set_brightness(uint8_t brightness);
esp_err_t m5unit_lcd_i2c_fill_rgb(uint8_t r, uint8_t g, uint8_t b);
esp_err_t m5unit_lcd_i2c_fill_rect_rgb(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye,
									   uint8_t r, uint8_t g, uint8_t b);
esp_err_t m5unit_lcd_i2c_show_accel(float ax_g, float ay_g, float az_g, uint16_t frame);
esp_err_t m5unit_lcd_i2c_test_step(uint16_t frame);
