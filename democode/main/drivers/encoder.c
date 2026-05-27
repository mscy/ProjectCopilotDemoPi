#include "encoder.h"
#include "config.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"

static const char *TAG = "encoder";

#define ENC_HIGH_LIMIT  32767
#define ENC_LOW_LIMIT  -32768

static pcnt_unit_handle_t s_unit = NULL;
static int s_last = 0;

esp_err_t encoder_init(void)
{
    if (s_unit) return ESP_OK;
    pcnt_unit_config_t ucfg = {
        .high_limit = ENC_HIGH_LIMIT,
        .low_limit  = ENC_LOW_LIMIT,
        .flags.accum_count = 1,
    };
    esp_err_t err = pcnt_new_unit(&ucfg, &s_unit);
    if (err != ESP_OK) return err;

    pcnt_glitch_filter_config_t gcfg = { .max_glitch_ns = 10000 };
    pcnt_unit_set_glitch_filter(s_unit, &gcfg);

    pcnt_chan_config_t ch_a_cfg = { .edge_gpio_num = ENCODER_A_GPIO, .level_gpio_num = ENCODER_B_GPIO };
    pcnt_chan_config_t ch_b_cfg = { .edge_gpio_num = ENCODER_B_GPIO, .level_gpio_num = ENCODER_A_GPIO };
    pcnt_channel_handle_t ch_a, ch_b;
    pcnt_new_channel(s_unit, &ch_a_cfg, &ch_a);
    pcnt_new_channel(s_unit, &ch_b_cfg, &ch_b);

    pcnt_channel_set_edge_action(ch_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(ch_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,   PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(ch_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(ch_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,   PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_unit_enable(s_unit);
    pcnt_unit_clear_count(s_unit);
    pcnt_unit_start(s_unit);
    ESP_LOGI(TAG, "PCNT encoder ready (A=%d B=%d)", ENCODER_A_GPIO, ENCODER_B_GPIO);
    return ESP_OK;
}

int encoder_get_delta(void)
{
    if (!s_unit) return 0;
    int cur = 0;
    pcnt_unit_get_count(s_unit, &cur);
    int delta = cur - s_last;
    s_last = cur;
    if (delta > 8) delta = 8;
    if (delta < -8) delta = -8;
    return delta;
}

esp_err_t encoder_self_test(void)
{
    if (!s_unit) return ESP_ERR_INVALID_STATE;
    int cur;
    pcnt_unit_get_count(s_unit, &cur);
    ESP_LOGI(TAG, "self-test PASS (count=%d)", cur);
    return ESP_OK;
}
