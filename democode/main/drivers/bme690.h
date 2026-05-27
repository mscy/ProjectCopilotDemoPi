#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    float gas_resistance_ohm;
    int iaq;
    uint8_t iaq_accuracy;
    bool iaq_from_bsec2;
} bme690_sample_t;

esp_err_t bme690_init(void);
esp_err_t bme690_self_test(void);
esp_err_t bme690_read(bme690_sample_t *out);
