#pragma once
#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    BTN_BOOT = 0,
    BTN_OPSKEY,
    BTN_COUNT
} btn_id_t;

typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT,
    BTN_EVT_LONG,    // > 3 seconds
} btn_event_t;

esp_err_t buttons_init(void);
btn_event_t buttons_poll(btn_id_t id);   // call ~50Hz
bool        buttons_is_down(btn_id_t id);
