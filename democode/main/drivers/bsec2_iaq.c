#include "bsec2_iaq.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <stdint.h>

#include "inc/bsec_datatypes.h"
#include "inc/bsec_interface.h"

static const char *TAG = "bsec2_iaq";

static bool s_logged_estimate = false;
static bool s_bsec_ready = false;
static float s_gas_baseline = 0.0f;
static uint32_t s_sample_count = 0;
static int s_last_bsec_iaq = 25;
static uint8_t s_last_bsec_accuracy = 0;
static uint32_t s_bsec_errors = 0;

static int clamp_iaq(int iaq)
{
    if (iaq < 25) return 25;
    if (iaq > 500) return 500;
    return iaq;
}

static int fallback_estimate_iaq(float temperature_c, float humidity_pct, float gas_ohm)
{
    // Very low resistance = sensor error
    if (gas_ohm <= 1000.0f) {
        return 500;
    }
    // Very high resistance = very clean air (BME68x: higher R = cleaner)
    if (gas_ohm > 5000000.0f) {
        return 25;
    }

    if (s_gas_baseline <= 0.0f) {
        s_gas_baseline = gas_ohm;
    }
    float alpha = (s_sample_count < 120) ? 0.10f : 0.02f;
    s_gas_baseline = (1.0f - alpha) * s_gas_baseline + alpha * gas_ohm;
    s_sample_count++;

    float ratio = gas_ohm / s_gas_baseline;
    int iaq;
    if (ratio >= 1.0f) {
        float good = ratio > 1.6f ? 1.6f : ratio;
        iaq = (int)(100.0f - (good - 1.0f) * 125.0f);
    } else {
        float bad = ratio < 0.2f ? 0.2f : ratio;
        iaq = (int)(100.0f + (1.0f - bad) * 500.0f);
    }

    float hum_penalty = 0.0f;
    if (humidity_pct < 35.0f) hum_penalty += (35.0f - humidity_pct) * 2.0f;
    if (humidity_pct > 65.0f) hum_penalty += (humidity_pct - 65.0f) * 2.0f;

    float temp_penalty = 0.0f;
    if (temperature_c < 18.0f) temp_penalty += (18.0f - temperature_c) * 2.0f;
    if (temperature_c > 30.0f) temp_penalty += (temperature_c - 30.0f) * 2.0f;

    iaq += (int)(hum_penalty + temp_penalty);
    return clamp_iaq(iaq);
}

esp_err_t bsec2_iaq_init(void)
{
    bsec_library_return_t st = bsec_init();
    if (st < BSEC_OK) {
        ESP_LOGE(TAG, "bsec_init err(%d), fallback", (int)st);
        s_bsec_ready = false;
    } else {
        bsec_sensor_configuration_t req[] = {
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_IAQ },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RAW_TEMPERATURE },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RAW_HUMIDITY },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RAW_PRESSURE },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RAW_GAS },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS },
            { .sample_rate = BSEC_SAMPLE_RATE_LP, .sensor_id = BSEC_OUTPUT_RUN_IN_STATUS },
        };
        bsec_sensor_configuration_t required[BSEC_MAX_PHYSICAL_SENSOR];
        uint8_t n_required = BSEC_MAX_PHYSICAL_SENSOR;
        st = bsec_update_subscription(req, sizeof(req)/sizeof(req[0]), required, &n_required);
        if (st < BSEC_OK) {
            ESP_LOGE(TAG, "bsec_subscribe err(%d), fallback", (int)st);
            s_bsec_ready = false;
        } else {
            if (st > BSEC_OK) {
                ESP_LOGW(TAG, "bsec_subscribe warn(%d), continuing", (int)st);
            }
            bsec_version_t ver = { 0 };
            (void)bsec_get_version(&ver);
            ESP_LOGI(TAG, "BSEC2 v%u.%u.%u.%u ready", ver.major, ver.minor, ver.major_bugfix, ver.minor_bugfix);
            s_bsec_ready = true;
            s_last_bsec_iaq = 25;
            s_last_bsec_accuracy = 0;
            s_bsec_errors = 0;
        }
    }

    if (!s_bsec_ready) {
        ESP_LOGI(TAG, "using gas-resistance IAQ estimate");
    }

    s_gas_baseline = 0.0f;
    s_sample_count = 0;
    s_logged_estimate = false;
    return ESP_OK;
}

esp_err_t bsec2_iaq_process(float temperature_c, float humidity_pct, float pressure_hpa,
                            float gas_resistance_ohm, bsec2_iaq_output_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    if (s_bsec_ready) {
        int64_t now_ns = esp_timer_get_time() * 1000LL;

        // Always feed all 4 physical inputs with current timestamp.
        // BSEC internally decides whether it's time to process based on
        // the subscribed sample rate. Warnings about timing are non-fatal.
        bsec_input_t in[4];
        uint8_t n_in = 0;

        in[n_in].time_stamp = now_ns;
        in[n_in].signal = pressure_hpa * 100.0f;
        in[n_in].signal_dimensions = 1;
        in[n_in].sensor_id = BSEC_INPUT_PRESSURE;
        n_in++;

        in[n_in].time_stamp = now_ns;
        in[n_in].signal = humidity_pct;
        in[n_in].signal_dimensions = 1;
        in[n_in].sensor_id = BSEC_INPUT_HUMIDITY;
        n_in++;

        in[n_in].time_stamp = now_ns;
        in[n_in].signal = temperature_c;
        in[n_in].signal_dimensions = 1;
        in[n_in].sensor_id = BSEC_INPUT_TEMPERATURE;
        n_in++;

        in[n_in].time_stamp = now_ns;
        in[n_in].signal = gas_resistance_ohm;
        in[n_in].signal_dimensions = 1;
        in[n_in].sensor_id = BSEC_INPUT_GASRESISTOR;
        n_in++;

        bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
        uint8_t n_out = BSEC_NUMBER_OUTPUTS;
        bsec_library_return_t st = bsec_do_steps(in, n_in, outputs, &n_out);
        if (st < BSEC_OK) {
            s_bsec_errors++;
            if (s_bsec_errors <= 3 || (s_bsec_errors % 10 == 0)) {
                ESP_LOGW(TAG, "bsec_do_steps err(%d) cnt=%lu", (int)st, (unsigned long)s_bsec_errors);
            }
            if (s_bsec_errors >= 20) {
                ESP_LOGE(TAG, "BSEC2 disabled after %lu errors", (unsigned long)s_bsec_errors);
                s_bsec_ready = false;
            }
        } else {
            if (st > BSEC_OK) {
                // Warnings (e.g. timing) are OK, just log once
                if (s_bsec_errors == 0) {
                    ESP_LOGI(TAG, "bsec_do_steps warn(%d)", (int)st);
                }
            }
            s_bsec_errors = 0;
            for (uint8_t i = 0; i < n_out; i++) {
                if (outputs[i].sensor_id == BSEC_OUTPUT_IAQ) {
                    int iaq = (int)lroundf(outputs[i].signal);
                    s_last_bsec_iaq = clamp_iaq(iaq);
                    s_last_bsec_accuracy = outputs[i].accuracy;
                    ESP_LOGI(TAG, "BSEC2 IAQ=%d acc=%u", s_last_bsec_iaq, (unsigned)s_last_bsec_accuracy);
                    break;
                }
            }
        }

        if (s_bsec_ready) {
            out->iaq = s_last_bsec_iaq;
            out->accuracy = s_last_bsec_accuracy;
            out->valid = true;
            out->from_bsec2 = true;
            return ESP_OK;
        }
    }

    if (!s_logged_estimate) {
        ESP_LOGI(TAG, "estimated IAQ active");
        s_logged_estimate = true;
    }

    out->iaq = fallback_estimate_iaq(temperature_c, humidity_pct, gas_resistance_ohm);
    out->accuracy = 0;
    out->valid = gas_resistance_ohm > 0.0f;
    out->from_bsec2 = false;
    return ESP_OK;
}
