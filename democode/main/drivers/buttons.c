#include "buttons.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const char *TAG = "buttons";

#define LONG_PRESS_US (5 * 1000 * 1000)

typedef struct {
    int gpio;
    bool active_low;
    bool was_down;
    int64_t down_ts;
    bool long_fired;
} btn_t;

static btn_t s_btns[BTN_COUNT] = {
    [BTN_BOOT]   = { BTN_BOOT_GPIO,   true, false, 0, false },
    [BTN_OPSKEY] = { BTN_OPSKEY_GPIO, true, false, 0, false },
};

esp_err_t buttons_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BTN_BOOT_GPIO) | (1ULL << BTN_OPSKEY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "buttons ready (BOOT=%d OPSKEY=%d)", BTN_BOOT_GPIO, BTN_OPSKEY_GPIO);
    return ESP_OK;
}

bool buttons_is_down(btn_id_t id)
{
    if (id >= BTN_COUNT) return false;
    int lvl = gpio_get_level(s_btns[id].gpio);
    return s_btns[id].active_low ? (lvl == 0) : (lvl != 0);
}

btn_event_t buttons_poll(btn_id_t id)
{
    if (id >= BTN_COUNT) return BTN_EVT_NONE;
    btn_t *b = &s_btns[id];
    bool down = buttons_is_down(id);
    int64_t now = esp_timer_get_time();
    btn_event_t ev = BTN_EVT_NONE;

    if (down && !b->was_down) {
        b->down_ts = now;
        b->long_fired = false;
    } else if (down && b->was_down) {
        if (!b->long_fired && (now - b->down_ts) > LONG_PRESS_US) {
            b->long_fired = true;
            ev = BTN_EVT_LONG;
        }
    } else if (!down && b->was_down) {
        if (!b->long_fired && (now - b->down_ts) > 30 * 1000) {
            ev = BTN_EVT_SHORT;
        }
    }
    b->was_down = down;
    return ev;
}
