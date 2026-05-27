#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include <stdbool.h>

// Lightweight UI placeholder. Full LVGL integration deferred.
esp_err_t display_boot_screen(void);
esp_err_t display_show_bme690(float temperature_c, float humidity_pct, float pressure_hpa,
							  float gas_resistance_ohm, int iaq, uint8_t iaq_accuracy,
							  bool iaq_from_bsec2);
esp_err_t display_show_bmi270(float ax_mps2, float ay_mps2, float az_mps2,
							  float gx_dps, float gy_dps, float gz_dps);
esp_err_t display_show_player(bool playing, uint8_t volume_pct,
							 const uint8_t *bars, size_t n_bars);
esp_err_t display_show_balance(float ax_mps2, float ay_mps2, float az_mps2);
esp_err_t display_show_i2c_scan(uint8_t progress, const uint8_t *addrs, size_t count);
esp_err_t display_show_m5_i2c_lcd(bool online, uint8_t addr, const uint8_t id[4],
								  uint8_t brightness, uint16_t frame, esp_err_t last_err);
esp_err_t display_show_mode(const char *name);
