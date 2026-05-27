#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int iaq;
    uint8_t accuracy;
    bool valid;
    bool from_bsec2;
} bsec2_iaq_output_t;

esp_err_t bsec2_iaq_init(void);
esp_err_t bsec2_iaq_process(float temperature_c, float humidity_pct, float pressure_hpa,
                            float gas_resistance_ohm, bsec2_iaq_output_t *out);