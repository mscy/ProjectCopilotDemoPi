#include "led_pwm.h"
#include "config.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "led_pwm";

#define PWM_TIMER       LEDC_TIMER_0
#define PWM_MODE        LEDC_LOW_SPEED_MODE
#define PWM_RES         LEDC_TIMER_10_BIT
#define PWM_FREQ_HZ     5000
#define PWM_MAX         ((1 << 10) - 1)

typedef enum { CH_R = 0, CH_G, CH_B, CH_BL } pwm_ch_t;

static const struct { ledc_channel_t ch; int gpio; bool enabled; } chans[] = {
    [CH_R]  = { LEDC_CHANNEL_0, LED_RED_GPIO,   true  },
    // GPIO33/34 are used by the in-package octal PSRAM on ESP32-S3-PICO-1-N8R8;
    // driving them as GPIO crashes PSRAM access → reboot loop. Leave disabled.
    [CH_G]  = { LEDC_CHANNEL_1, LED_GREEN_GPIO, false },
    [CH_B]  = { LEDC_CHANNEL_2, LED_BLUE_GPIO,  false },
    [CH_BL] = { LEDC_CHANNEL_3, LCD_BL_GPIO,    true  },
};

static esp_err_t set_duty(pwm_ch_t c, uint32_t duty)
{
    if (!chans[c].enabled) return ESP_OK;
    if (duty > PWM_MAX) duty = PWM_MAX;
    esp_err_t err = ledc_set_duty(PWM_MODE, chans[c].ch, duty);
    if (err != ESP_OK) return err;
    return ledc_update_duty(PWM_MODE, chans[c].ch);
}

esp_err_t led_pwm_init(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode = PWM_MODE,
        .duty_resolution = PWM_RES,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&tcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer cfg: %s", esp_err_to_name(err));
        return err;
    }
    for (size_t i = 0; i < sizeof(chans)/sizeof(chans[0]); i++) {
        if (!chans[i].enabled) continue;
        ledc_channel_config_t ccfg = {
            .gpio_num = chans[i].gpio,
            .speed_mode = PWM_MODE,
            .channel = chans[i].ch,
            .timer_sel = PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        err = ledc_channel_config(&ccfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "channel %d: %s", (int)i, esp_err_to_name(err));
            return err;
        }
    }
    ESP_LOGI(TAG, "LEDC ready (R=%d BL=%d; G/B disabled, octal PSRAM conflict)",
             LED_RED_GPIO, LCD_BL_GPIO);
    return ESP_OK;
}

esp_err_t led_pwm_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    set_duty(CH_R, (r * PWM_MAX) / 255);
    set_duty(CH_G, (g * PWM_MAX) / 255);
    set_duty(CH_B, (b * PWM_MAX) / 255);
    return ESP_OK;
}

esp_err_t lcd_backlight_set(uint8_t pct)
{
    if (pct > 100) pct = 100;
    return set_duty(CH_BL, (pct * PWM_MAX) / 100);
}
