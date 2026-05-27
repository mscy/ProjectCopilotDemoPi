#pragma once
#include "esp_err.h"

esp_err_t power_init(void);
void      power_enter_deep_sleep(void);     // wake on OpsKey
int       power_battery_pct(void);
