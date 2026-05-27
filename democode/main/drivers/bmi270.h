#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef struct {
    float ax_g, ay_g, az_g;
    float gx_dps, gy_dps, gz_dps;
} bmi270_sample_t;

esp_err_t bmi270_init(void);
esp_err_t bmi270_self_test(void);
esp_err_t bmi270_read(bmi270_sample_t *out);
