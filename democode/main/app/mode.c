#include "mode.h"
#include "ws2812b.h"
#include "esp_log.h"

static const char *TAG = "mode";
static app_mode_t s_mode = MODE_STANDBY;

static const char *NAMES[] = {
    "SDCARD", "RADIO", "STANDBY", "SLEEP"
};

static void apply_led(app_mode_t m)
{
    switch (m) {
        case MODE_SDCARD:    ws2812_set(0, 0,   80,  0);  break;  // green
        case MODE_RADIO:     ws2812_set(0, 80,  60,  0);  break;  // yellow
        case MODE_STANDBY:   ws2812_set(0, 30,  30,  30); break;  // white dim
        case MODE_SLEEP:     ws2812_set(0, 0,   0,   0);  break;
        default: break;
    }
    ws2812_refresh();
}

esp_err_t mode_init(app_mode_t initial)
{
    s_mode = initial;
    apply_led(s_mode);
    ESP_LOGI(TAG, "initial mode = %s", NAMES[s_mode]);
    return ESP_OK;
}

app_mode_t mode_current(void) { return s_mode; }

esp_err_t mode_set(app_mode_t m)
{
    if (m >= MODE_COUNT) return ESP_ERR_INVALID_ARG;
    if (m == s_mode) return ESP_OK;
    ESP_LOGI(TAG, "mode %s -> %s", NAMES[s_mode], NAMES[m]);
    s_mode = m;
    apply_led(s_mode);
    return ESP_OK;
}

esp_err_t mode_cycle(void)
{
    // Cycle through user-facing modes only (skip SLEEP)
    app_mode_t next = (s_mode + 1) % MODE_SLEEP;
    return mode_set(next);
}

const char *mode_name(app_mode_t m)
{
    return (m < MODE_COUNT) ? NAMES[m] : "?";
}
