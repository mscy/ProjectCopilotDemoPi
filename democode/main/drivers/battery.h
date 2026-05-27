#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t battery_init(void);
esp_err_t battery_self_test(void);
int       battery_read_mv(void);    // returns mV at battery, or <0 on error
int       battery_read_pct(void);   // 0..100
