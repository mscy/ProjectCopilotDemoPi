#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#include "config.h"
#include "drivers/i2c_bus.h"
#include "drivers/led_pwm.h"
#include "drivers/lcd.h"
#include "drivers/bme690.h"
#include "drivers/bmi270.h"
#include "drivers/max98357a.h"
#include "drivers/ws2812b.h"
#include "drivers/encoder.h"
#include "drivers/buttons.h"
#include "drivers/sdcard.h"
#include "drivers/battery.h"
#include "app/mode.h"
#include "app/power.h"
#include "app/display.h"
#include "app/mp3_player.h"

static const char *TAG = "app";

typedef enum {
    SENSOR_TAB_BME690 = 0,
    SENSOR_TAB_BMI270,
    SENSOR_TAB_BALANCE,
    SENSOR_TAB_PLAYER,
    SENSOR_TAB_I2C_SCAN,
    SENSOR_TAB__COUNT,
} sensor_tab_t;

static sensor_tab_t s_sensor_tab = SENSOR_TAB_BME690;
static TaskHandle_t s_sensor_task = NULL;
static QueueHandle_t s_tick_queue = NULL;

#define PLAYER_TAB_BARS 16

static void tick_task(void *arg)
{
    uint8_t evt;
    while (1) {
        if (xQueueReceive(s_tick_queue, &evt, portMAX_DELAY) == pdTRUE) {
            // Drain any queued ticks so we only play one sound per burst.
            while (xQueueReceive(s_tick_queue, &evt, 0) == pdTRUE) {}
            (void)audio_play_tick();
        }
    }
}
static bme690_sample_t s_latest_env;
static bmi270_sample_t s_latest_imu;
static bool s_have_env = false;
static bool s_have_imu = false;

// I2C scanner state
#define I2C_SCAN_MAX_DEVS 16
static uint8_t  s_i2c_addrs[I2C_SCAN_MAX_DEVS];
static size_t   s_i2c_count = 0;
static uint8_t  s_i2c_progress = 0;
static bool     s_i2c_scanning = false;
static bool     s_i2c_done = false;

static void display_active_sensor_tab(void)
{
    uint8_t bars[PLAYER_TAB_BARS] = { 0 };
    if (s_sensor_tab == SENSOR_TAB_BME690) {
        if (s_have_env) {
            display_show_bme690(s_latest_env.temperature_c,
                                s_latest_env.humidity_pct,
                                s_latest_env.pressure_hpa,
                                s_latest_env.gas_resistance_ohm,
                                s_latest_env.iaq,
                                s_latest_env.iaq_accuracy,
                                s_latest_env.iaq_from_bsec2);
        }
    } else if (s_sensor_tab == SENSOR_TAB_BMI270) {
        if (s_have_imu) {
            display_show_bmi270(s_latest_imu.ax_g * 9.80665f,
                                s_latest_imu.ay_g * 9.80665f,
                                s_latest_imu.az_g * 9.80665f,
                                s_latest_imu.gx_dps,
                                s_latest_imu.gy_dps,
                                s_latest_imu.gz_dps);
        }
    } else if (s_sensor_tab == SENSOR_TAB_BALANCE) {
        if (s_have_imu) {
            display_show_balance(s_latest_imu.ax_g * 9.80665f,
                                 s_latest_imu.ay_g * 9.80665f,
                                 s_latest_imu.az_g * 9.80665f);
        }
    } else if (s_sensor_tab == SENSOR_TAB_PLAYER) {
        mp3_player_get_spectrum(bars, PLAYER_TAB_BARS);
        display_show_player(mp3_player_has_track(), audio_get_volume(), bars, PLAYER_TAB_BARS);

        uint8_t bass_e = 0, mid_e = 0, treble_e = 0;
        mp3_player_get_band_energy(&bass_e, &mid_e, &treble_e);
        static uint16_t s_hue = 0;
        s_hue = (uint16_t)((s_hue + (bass_e > 55 ? 18 : 3)) % 360);
        uint8_t energy = (uint8_t)((bass_e + mid_e + treble_e) / 3);
        if (energy < 8) energy = 8;
        uint8_t v = (uint8_t)((uint16_t)energy * 220 / 100);
        if (v > 220) v = 220;
        uint8_t region = (uint8_t)(s_hue / 60);
        uint8_t rem    = (uint8_t)((s_hue % 60) * 255 / 60);
        uint8_t hq     = (uint8_t)((uint16_t)v * (255 - rem) / 255);
        uint8_t ht     = (uint8_t)((uint16_t)v * rem / 255);
        uint8_t led_r, led_g, led_b;
        switch (region) {
            case 0:  led_r=v;   led_g=ht;  led_b=0;   break;
            case 1:  led_r=hq;  led_g=v;   led_b=0;   break;
            case 2:  led_r=0;   led_g=v;   led_b=ht;  break;
            case 3:  led_r=0;   led_g=hq;  led_b=v;   break;
            case 4:  led_r=ht;  led_g=0;   led_b=v;   break;
            default: led_r=v;   led_g=0;   led_b=hq;  break;
        }
        ws2812_set(0, led_r, led_g, led_b);
        ws2812_refresh();
    } else if (s_sensor_tab == SENSOR_TAB_I2C_SCAN) {
        // Kick off scan on first entry
        if (!s_i2c_scanning && !s_i2c_done) {
            s_i2c_scanning = true;
            s_i2c_count = 0;
            s_i2c_progress = 0;
            led_pwm_set_rgb(0, 0, 0);
            ws2812_set(0, 0, 0, 0);
            ws2812_refresh();
            (void)audio_play_scan_start();   // rising boop — scan starts
            const uint8_t addr_lo = 0x01, addr_hi = 0x7F;
            const uint8_t total = addr_hi - addr_lo + 1;
            // LED blink phases (in scan iterations, ~12 ms each).
            // Red: fast blink (~80 ms period). Blue: slow blink (~640 ms period).
            // GPIO33/34 (green/blue PWM) are reserved by octal PSRAM, so
            // route the blue indicator through the WS2812B instead.
            const uint8_t red_period  = 6;
            const uint8_t blue_period = 48;
            bool aborted = false;
            for (uint8_t a = addr_lo; a <= addr_hi; a++) {
                if (s_sensor_tab != SENSOR_TAB_I2C_SCAN) { aborted = true; break; }
                s_i2c_progress = (uint8_t)(((uint32_t)(a - addr_lo + 1) * 100) / total);
                uint8_t step = (uint8_t)(a - addr_lo);
                bool red_on  = !((step / red_period)  & 1);
                bool blue_on = !((step / blue_period) & 1);
                led_pwm_set_rgb(red_on ? 255 : 0, 0, 0);
                ws2812_set(0, 0, 0, blue_on ? 80 : 0);
                ws2812_refresh();
                if (i2c_master_probe(i2c_bus_handle(), a, 10) == ESP_OK) {
                    if (s_i2c_count < I2C_SCAN_MAX_DEVS) {
                        s_i2c_addrs[s_i2c_count++] = a;
                    }
                }
                display_show_i2c_scan(s_i2c_progress, s_i2c_addrs, s_i2c_count);
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            s_i2c_scanning = false;
            if (aborted) {
                s_i2c_done = false;
                led_pwm_set_rgb(0, 0, 0);
                ws2812_set(0, 0, 0, 0);
                ws2812_refresh();
                return;
            }
            s_i2c_progress = 100;
            s_i2c_done = true;
            led_pwm_set_rgb(0, 0, 0);
            ws2812_set(0, 80, 0, 0);
            ws2812_refresh();
            (void)audio_play_scan_done();    // ding-dong — scan complete
            ESP_LOGI(TAG, "I2C scan complete: %u device(s)", (unsigned)s_i2c_count);
        }
        display_show_i2c_scan(s_i2c_progress, s_i2c_addrs, s_i2c_count);
    }
}

static void log_chip_info(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);
    ESP_LOGI(TAG, "%s — fw %s", DEVICE_NAME, FIRMWARE_VERSION);
    ESP_LOGI(TAG, "ESP32-S3, %d cores, rev %d", info.cores, info.revision);
    ESP_LOGI(TAG, "PSRAM free: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void boot_led_test(void)
{
    led_pwm_set_rgb(255, 0, 0); vTaskDelay(pdMS_TO_TICKS(300));
    led_pwm_set_rgb(0, 255, 0); vTaskDelay(pdMS_TO_TICKS(300));
    led_pwm_set_rgb(0, 0, 255); vTaskDelay(pdMS_TO_TICKS(300));
    led_pwm_set_rgb(0, 0, 0);
}

static void sensor_task(void *arg)
{
    bme690_sample_t env;
    bmi270_sample_t imu;
    // IMU refresh ~10Hz; BME690 (slow forced-mode) refresh ~1Hz.
    const TickType_t period = pdMS_TO_TICKS(100);
    const int bme_every = 10;
    int tick = 0;
    while (1) {
        if (tick % bme_every == 0) {
            if (bme690_read(&env) == ESP_OK) {
                s_latest_env = env;
                s_have_env = true;
                ESP_LOGI("sensor", "T=%.2fC RH=%.1f%% P=%.1fhPa VOC=%.0fohm IAQ=%d A=%u (%s)",
                         env.temperature_c, env.humidity_pct, env.pressure_hpa,
                         env.gas_resistance_ohm, env.iaq, (unsigned)env.iaq_accuracy,
                         env.iaq_from_bsec2 ? "BSEC2" : "estimate");
            }
        }
        esp_err_t imu_err = bmi270_read(&imu);
        if (imu_err == ESP_OK) {
            s_latest_imu = imu;
            s_have_imu = true;
            if (tick % 10 == 0) {
                ESP_LOGI("sensor", "AX=%.2f AY=%.2f AZ=%.2f m/s2 GX=%.2f GY=%.2f GZ=%.2f dps",
                         imu.ax_g * 9.80665f, imu.ay_g * 9.80665f, imu.az_g * 9.80665f,
                         imu.gx_dps, imu.gy_dps, imu.gz_dps);
            }
        } else {
            ESP_LOGW("sensor", "BMI270 read failed: %s", esp_err_to_name(imu_err));
        }
        display_active_sensor_tab();
        tick++;
        ulTaskNotifyTake(pdTRUE, period);
    }
}

static void hsv_to_rgb(uint16_t h_deg, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // h: 0..359, s=v=255
    uint8_t region = h_deg / 60;
    uint16_t rem = (h_deg - region * 60) * 255 / 60;
    uint8_t p = 0;
    uint8_t q = 255 - rem;
    uint8_t t = rem;
    switch (region) {
    case 0: *r = 255; *g = t;   *b = p;   break;
    case 1: *r = q;   *g = 255; *b = p;   break;
    case 2: *r = p;   *g = 255; *b = t;   break;
    case 3: *r = p;   *g = q;   *b = 255; break;
    case 4: *r = t;   *g = p;   *b = 255; break;
    default:*r = 255; *g = p;   *b = q;   break;
    }
}

static void control_task(void *arg)
{
    static uint16_t s_hue = 0; // 0..359
    while (1) {
        int delta = encoder_get_delta();
        if (delta != 0) {
            if (s_sensor_tab == SENSOR_TAB_PLAYER) {
                int volume = (int)audio_get_volume() + delta * 3;
                if (volume < 0) volume = 0;
                if (volume > 100) volume = 100;
                audio_set_volume((uint8_t)volume);
            } else {
                int h = (int)s_hue + delta * 12;
                while (h < 0) h += 360;
                while (h >= 360) h -= 360;
                s_hue = (uint16_t)h;
                uint8_t r = 0, g = 0, b = 0;
                hsv_to_rgb(s_hue, &r, &g, &b);
                ws2812_set(0, r / 4, g / 4, b / 4);
                ws2812_refresh();
                ESP_LOGI(TAG, "hue=%u rgb=(%u,%u,%u)", s_hue, r, g, b);
            }

            if (s_sensor_tab != SENSOR_TAB_PLAYER && s_tick_queue) {
                uint8_t evt = 1;
                int n = delta < 0 ? -delta : delta;
                if (n > 4) n = 4;
                for (int i = 0; i < n; i++) {
                    if (xQueueSend(s_tick_queue, &evt, 0) != pdTRUE) break;
                }
            }
        }
        btn_event_t op = buttons_poll(BTN_OPSKEY);
        if (op == BTN_EVT_SHORT) {
            if (s_sensor_tab == SENSOR_TAB_PLAYER) {
                mp3_player_set_enabled(false);
                vTaskDelay(pdMS_TO_TICKS(60));
            }
            // Reset I2C scan state so next visit re-scans
            s_i2c_done = false;
            s_i2c_scanning = false;
            led_pwm_set_rgb(0, 0, 0);
            ws2812_set(0, 0, 0, 0);
            ws2812_refresh();
            esp_err_t click_err = audio_play_click();
            if (click_err != ESP_OK) {
                ESP_LOGW(TAG, "OpsKey click failed: %s", esp_err_to_name(click_err));
            }
            s_sensor_tab = (sensor_tab_t)((s_sensor_tab + 1) % SENSOR_TAB__COUNT);
            mp3_player_set_enabled(s_sensor_tab == SENSOR_TAB_PLAYER);
            const char *tab_name =
                (s_sensor_tab == SENSOR_TAB_BME690)    ? "BME690"   :
                (s_sensor_tab == SENSOR_TAB_BMI270)    ? "BMI270"   :
                (s_sensor_tab == SENSOR_TAB_BALANCE)   ? "BALANCE"  :
                (s_sensor_tab == SENSOR_TAB_PLAYER)    ? "PLAYER"   :
                                                         "I2C SCAN";
            ESP_LOGI(TAG, "sensor tab=%s", tab_name);

            // External strip: per-tab status color (dim).
            if (ws2812_strip_count() > 0) {
                uint8_t r = 0, g = 0, b = 0;
                switch (s_sensor_tab) {
                    case SENSOR_TAB_BME690:   r = 0;  g = 30; b = 8;  break; // teal
                    case SENSOR_TAB_BMI270:   r = 30; g = 12; b = 0;  break; // amber
                    case SENSOR_TAB_BALANCE:  r = 24; g = 0;  b = 24; break; // magenta
                    case SENSOR_TAB_PLAYER:   r = 0;  g = 0;  b = 40; break; // blue
                    case SENSOR_TAB_I2C_SCAN: r = 30; g = 30; b = 30; break; // white
                    default: break;
                }
                ws2812_strip_fill(r, g, b);
                ws2812_strip_refresh();
            }
            if (s_sensor_task) xTaskNotifyGive(s_sensor_task);
        } else if (op == BTN_EVT_LONG) {
            ESP_LOGW(TAG, "OpsKey long press → deep sleep");
            mode_set(MODE_SLEEP);
            vTaskDelay(pdMS_TO_TICKS(200));
            power_enter_deep_sleep();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void power_task(void *arg)
{
    while (1) {
        int mv = battery_read_mv();
        int pct = battery_read_pct();
        ESP_LOGI("power", "VBAT=%d mV (%d%%)", mv, pct);
        if (mv > 0 && mv < (int)(BATT_VOLTAGE_SHUT * 1000)) {
            ESP_LOGE("power", "Critical battery — entering deep sleep");
            power_enter_deep_sleep();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    log_chip_info();

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ---- Phase 1: HAL ----
    ESP_ERROR_CHECK(led_pwm_init());
    ESP_ERROR_CHECK(i2c_bus_init());
    i2c_bus_scan();

    // ---- Phase 2: drivers ----
    if (lcd_init() == ESP_OK) {
        display_boot_screen();
#if LCD_DIAGNOSTIC_LOOP
        lcd_diagnostic_loop();
#endif
    }

    if (audio_init() == ESP_OK) {
        audio_set_volume(40);
        audio_play_chime();   // single startup chime per reset
    }

    if (mp3_player_init() != ESP_OK) {
        ESP_LOGW(TAG, "player init failed");
    }
    mp3_player_set_enabled(false);

    vTaskDelay(pdMS_TO_TICKS(2000));

    boot_led_test();

    if (ws2812_init(1) == ESP_OK) ws2812_self_test();

    if (ws2812_strip_init(WS2812_STRIP_GPIO, WS2812_STRIP_COUNT) == ESP_OK) {
        ws2812_strip_rainbow_sweep(1, 12);
        ws2812_strip_fill(0, 30, 8);   // boot tab = BME690 (teal)
        ws2812_strip_refresh();
    }

    if (bme690_init() == ESP_OK) bme690_self_test();
    if (bmi270_init() == ESP_OK) bmi270_self_test();

    encoder_init();
    buttons_init();

    if (sdcard_init() != ESP_OK) {
        ESP_LOGI(TAG, "SD card unavailable on this board revision");
    }

    if (battery_init() == ESP_OK) battery_self_test();
    power_init();

    // ---- App ----
    mode_init(MODE_STANDBY);

    s_tick_queue = xQueueCreate(8, sizeof(uint8_t));
    xTaskCreatePinnedToCore(sensor_task,  "sensor",  8192, NULL, 5, &s_sensor_task, 0);
    xTaskCreatePinnedToCore(control_task, "control", 4096, NULL, 6, NULL, 0);
    xTaskCreatePinnedToCore(tick_task,    "tick",    4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(power_task,   "power",   3072, NULL, 4, NULL, 0);

    ESP_LOGI(TAG, "boot complete");
}
