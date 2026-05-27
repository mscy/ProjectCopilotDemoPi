#include "power.h"
#include "config.h"
#include "battery.h"
#include "lcd.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "power";

esp_err_t power_init(void)
{
    return ESP_OK;
}

int power_battery_pct(void)
{
    return battery_read_pct();
}

void power_enter_deep_sleep(void)
{
    ESP_LOGW(TAG, "Entering deep sleep");
    lcd_off();

    // Wait for OpsKey to be released — otherwise ext0 (level=0 wake)
    // would fire immediately and look like a reboot.
    int64_t t0 = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)BTN_OPSKEY_GPIO) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (esp_timer_get_time() - t0 > 10LL * 1000 * 1000) break; // safety: 10 s
    }
    // Small debounce so a release bounce doesn't immediately re-wake.
    vTaskDelay(pdMS_TO_TICKS(80));

    // Disable any prior wake source, then enable a clean ext0 on OpsKey low.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    rtc_gpio_init((gpio_num_t)BTN_OPSKEY_GPIO);
    rtc_gpio_set_direction((gpio_num_t)BTN_OPSKEY_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_OPSKEY_GPIO);
    rtc_gpio_pullup_en((gpio_num_t)BTN_OPSKEY_GPIO);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_OPSKEY_GPIO, 0);

    // Hold the pull-up across deep sleep so a floating press isn't seen.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    esp_deep_sleep_start();
}
