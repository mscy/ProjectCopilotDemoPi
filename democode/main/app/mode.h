#pragma once
#include "esp_err.h"

typedef enum {
    MODE_SDCARD = 0,
    MODE_RADIO,
    MODE_STANDBY,
    MODE_SLEEP,
    MODE_COUNT
} app_mode_t;

esp_err_t mode_init(app_mode_t initial);
app_mode_t mode_current(void);
esp_err_t mode_set(app_mode_t m);
esp_err_t mode_cycle(void);
const char *mode_name(app_mode_t m);
