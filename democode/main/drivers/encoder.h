#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t encoder_init(void);
int       encoder_get_delta(void);   // signed steps since last call
esp_err_t encoder_self_test(void);
