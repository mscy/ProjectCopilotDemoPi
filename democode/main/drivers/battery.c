#include "battery.h"
#include "config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;

#define BATT_ADC_UNIT     ADC_UNIT_1
#define BATT_ADC_CHANNEL  ADC_CHANNEL_0     // GPIO1 == ADC1_CH0
#define BATT_ATTEN        ADC_ATTEN_DB_12

esp_err_t battery_init(void)
{
    if (s_adc) return ESP_OK;
    adc_oneshot_unit_init_cfg_t init = { .unit_id = BATT_ADC_UNIT };
    esp_err_t err = adc_oneshot_new_unit(&init, &s_adc);
    if (err != ESP_OK) return err;

    adc_oneshot_chan_cfg_t cfg = {
        .atten = BATT_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc, BATT_ADC_CHANNEL, &cfg);
    if (err != ESP_OK) return err;

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATT_ADC_UNIT,
        .atten = BATT_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "calibration unavailable; using raw scaling");
    }
    ESP_LOGI(TAG, "battery ADC ready (gpio=%d)", BATT_ADC_GPIO);
    return ESP_OK;
}

int battery_read_mv(void)
{
    if (!s_adc) return -1;
    int sum = 0;
    int valid = 0;
    for (int i = 0; i < 10; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, BATT_ADC_CHANNEL, &raw) != ESP_OK) continue;
        int mv = 0;
        if (s_cali) {
            if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) continue;
        } else {
            mv = raw;  // uncalibrated fallback
        }
        sum += mv;
        valid++;
    }
    if (!valid) return -1;
    int avg_mv = sum / valid;
    return (int)(avg_mv * BATT_DIVIDER_RATIO);
}

int battery_read_pct(void)
{
    int mv = battery_read_mv();
    if (mv < 0) return -1;
    int full = (int)(BATT_VOLTAGE_FULL * 1000);
    int empty = (int)(BATT_VOLTAGE_SHUT * 1000);
    if (mv >= full) return 100;
    if (mv <= empty) return 0;
    return (mv - empty) * 100 / (full - empty);
}

esp_err_t battery_self_test(void)
{
    int mv = battery_read_mv();
    if (mv < 0) {
        ESP_LOGE(TAG, "self-test FAIL");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "self-test PASS (VBAT=%d mV, %d%%)", mv, battery_read_pct());
    return ESP_OK;
}
