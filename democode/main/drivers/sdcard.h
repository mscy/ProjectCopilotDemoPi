#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t sdcard_init(void);          // returns ESP_ERR_NOT_FOUND if no card
bool      sdcard_is_mounted(void);
