// BME690 minimal driver (forced-mode, T/H/P/gas).
// NOTE: official BSEC2 is a closed-source package and is routed via bsec2_iaq.
// Compensation math follows the BME68x datasheet (BME690 is register-compatible
// with BME680/BME688 for temperature/humidity/pressure).
#include "bme690.h"
#include "bsec2_iaq.h"
#include "i2c_bus.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "bme690";

#define REG_CHIP_ID     0xD0
#define REG_RESET       0xE0
#define REG_CTRL_HUM    0x72
#define REG_CTRL_MEAS   0x74
#define REG_CONFIG      0x75
#define REG_CTRL_GAS_0  0x70
#define REG_CTRL_GAS_1  0x71
#define REG_GAS_WAIT_0  0x64
#define REG_RES_HEAT_0  0x5A
#define REG_FIELD0      0x1D
#define REG_PRESS_MSB   0x1F
#define REG_TEMP_MSB    0x22
#define REG_HUM_MSB     0x25
#define REG_GAS_R_MSB   0x2A
#define REG_CALIB_00    0x89
#define REG_CALIB_26    0xE1

#define CHIP_ID_EXPECTED 0x61
#define FIELD_COUNT      3
#define FIELD_LENGTH     17
#define FIELD_STRIDE     17
#define FIELD_STATUS_OFF 0
#define FIELD_PRESS_OFF  2
#define FIELD_TEMP_OFF   5
#define FIELD_HUM_OFF    8
#define FIELD_GAS_OFF    13
#define NEW_DATA_BIT     0x80
#define GAS_VALID_BIT    0x20
#define HEAT_STAB_BIT    0x10

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_logged_gas_saturation = false;

typedef struct {
    uint16_t par_t1;
    int16_t par_t2;
    int8_t par_t3;
    uint16_t par_p1;
    int16_t par_p2;
    int8_t par_p3;
    int16_t par_p4;
    int16_t par_p5;
    int8_t par_p6;
    int8_t par_p7;
    int16_t par_p8;
    int16_t par_p9;
    uint8_t par_p10;
    uint16_t par_h1;
    uint16_t par_h2;
    int8_t par_h3;
    int8_t par_h4;
    int8_t par_h5;
    uint8_t par_h6;
    int8_t par_h7;
    int8_t par_gh1;
    int16_t par_gh2;
    int8_t par_gh3;
    uint8_t res_heat_range;
    int8_t res_heat_val;
    float t_fine;
} bme690_calib_t;

static bme690_calib_t s_calib;

static uint16_t u16_le(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static int16_t s16_le(const uint8_t *buf)
{
    return (int16_t)u16_le(buf);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 100);
}
static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 100);
}

static esp_err_t wait_for_forced_measurement(void)
{
    for (int i = 0; i < 100; i++) {
        uint8_t ctrl_meas = 0;
        esp_err_t err = reg_read(REG_CTRL_MEAS, &ctrl_meas, 1);
        if (err != ESP_OK) return err;
        if ((ctrl_meas & 0x03) == 0) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ESP_LOGW(TAG, "forced measurement timeout");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t read_calibration(void)
{
    uint8_t c0[25];
    uint8_t c1[16];
    esp_err_t err = reg_read(REG_CALIB_00, c0, sizeof(c0));
    if (err != ESP_OK) return err;
    err = reg_read(REG_CALIB_26, c1, sizeof(c1));
    if (err != ESP_OK) return err;

    s_calib.par_t2 = s16_le(&c0[1]);      // 0x8A/0x8B
    s_calib.par_t3 = (int8_t)c0[3];       // 0x8C
    s_calib.par_p1 = u16_le(&c0[5]);      // 0x8E/0x8F
    s_calib.par_p2 = s16_le(&c0[7]);      // 0x90/0x91
    s_calib.par_p3 = (int8_t)c0[9];       // 0x92
    s_calib.par_p4 = s16_le(&c0[11]);     // 0x94/0x95
    s_calib.par_p5 = s16_le(&c0[13]);     // 0x96/0x97
    s_calib.par_p7 = (int8_t)c0[15];      // 0x98
    s_calib.par_p6 = (int8_t)c0[16];      // 0x99
    s_calib.par_p8 = s16_le(&c0[19]);     // 0x9C/0x9D
    s_calib.par_p9 = s16_le(&c0[21]);     // 0x9E/0x9F
    s_calib.par_p10 = c0[23];             // 0xA0

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, c0, sizeof(c0), ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, c1, sizeof(c1), ESP_LOG_INFO);

    s_calib.par_h2 = (uint16_t)(((uint16_t)c1[0] << 4) | (c1[1] >> 4));
    s_calib.par_h1 = (uint16_t)(((uint16_t)c1[2] << 4) | (c1[1] & 0x0F));
    s_calib.par_h3 = (int8_t)c1[3];
    s_calib.par_h4 = (int8_t)c1[4];
    s_calib.par_h5 = (int8_t)c1[5];
    s_calib.par_h6 = c1[6];
    s_calib.par_h7 = (int8_t)c1[7];
    s_calib.par_t1 = u16_le(&c1[8]);      // 0xE9/0xEA
    s_calib.par_gh2 = s16_le(&c1[10]);     // 0xEB/0xEC
    s_calib.par_gh1 = (int8_t)c1[12];      // 0xED
    s_calib.par_gh3 = (int8_t)c1[13];      // 0xEE

    // res_heat_range: register 0x02, bits 5:4
    uint8_t rhr = 0;
    err = reg_read(0x02, &rhr, 1);
    if (err != ESP_OK) return err;
    s_calib.res_heat_range = (rhr >> 4) & 0x03;

    // res_heat_val: register 0x00 (signed)
    uint8_t rhv = 0;
    err = reg_read(0x00, &rhv, 1);
    if (err != ESP_OK) return err;
    s_calib.res_heat_val = (int8_t)rhv;

    ESP_LOGI(TAG, "calibration loaded (T1=%u P1=%u H1=%u H2=%u GH1=%d GH2=%d GH3=%d RHR=%u RHV=%d)",
             s_calib.par_t1, s_calib.par_p1, s_calib.par_h1, s_calib.par_h2,
             s_calib.par_gh1, s_calib.par_gh2, s_calib.par_gh3,
             s_calib.res_heat_range, s_calib.res_heat_val);
    return ESP_OK;
}

static float compensate_temperature(uint32_t adc_t)
{
    float var1 = (((float)adc_t / 16384.0f) - ((float)s_calib.par_t1 / 1024.0f)) *
                 (float)s_calib.par_t2;
    float var2 = (((float)adc_t / 131072.0f) - ((float)s_calib.par_t1 / 8192.0f));
    var2 = var2 * var2 * ((float)s_calib.par_t3 * 16.0f);
    s_calib.t_fine = var1 + var2;
    return s_calib.t_fine / 5120.0f;
}

static float compensate_pressure(uint32_t adc_p)
{
    float var1 = (s_calib.t_fine / 2.0f) - 64000.0f;
    float var2 = var1 * var1 * ((float)s_calib.par_p6 / 131072.0f);
    var2 += var1 * (float)s_calib.par_p5 * 2.0f;
    var2 = (var2 / 4.0f) + ((float)s_calib.par_p4 * 65536.0f);
    var1 = ((((float)s_calib.par_p3 * var1 * var1) / 16384.0f) +
            ((float)s_calib.par_p2 * var1)) / 524288.0f;
    var1 = (1.0f + (var1 / 32768.0f)) * (float)s_calib.par_p1;
    if (var1 == 0.0f) return 0.0f;

    float pressure = 1048576.0f - (float)adc_p;
    pressure = ((pressure - (var2 / 4096.0f)) * 6250.0f) / var1;
    var1 = ((float)s_calib.par_p9 * pressure * pressure) / 2147483648.0f;
    var2 = pressure * ((float)s_calib.par_p8 / 32768.0f);
    float var3 = (pressure / 256.0f) * (pressure / 256.0f) * (pressure / 256.0f) *
                 ((float)s_calib.par_p10 / 131072.0f);
    pressure += (var1 + var2 + var3 + ((float)s_calib.par_p7 * 128.0f)) / 16.0f;
    return pressure / 100.0f;
}

static float compensate_humidity(uint16_t adc_h)
{
    float temp_c = s_calib.t_fine / 5120.0f;
    float var1 = (float)adc_h - (((float)s_calib.par_h1 * 16.0f) +
                 (((float)s_calib.par_h3 / 2.0f) * temp_c));
    float var2 = var1 * (((float)s_calib.par_h2 / 262144.0f) *
                 (1.0f + (((float)s_calib.par_h4 / 16384.0f) * temp_c) +
                 (((float)s_calib.par_h5 / 1048576.0f) * temp_c * temp_c)));
    float var3 = (float)s_calib.par_h6 / 16384.0f;
    float var4 = (float)s_calib.par_h7 / 2097152.0f;
    float hum = var2 + ((var3 + (var4 * temp_c)) * var2 * var2);
    if (hum > 100.0f) hum = 100.0f;
    if (hum < 0.0f) hum = 0.0f;
    return hum;
}

static float compensate_gas_resistance(uint16_t adc_gas, uint8_t gas_range)
{
    if (gas_range > 15) return 0.0f;

    float var1 = (float)(262144U >> gas_range);
    float var2 = ((float)adc_gas - 512.0f) * 3.0f;
    var2 = 4096.0f + var2;
    if (var2 <= 0.0f) return 0.0f;
    return (1000000.0f * var1) / var2;
}

// Calculate heater resistance register value for target temperature (°C).
// Formula from BME68x datasheet section "Heater resistance".
static uint8_t calc_res_heat(uint16_t target_temp, float amb_temp)
{
    float var1 = ((float)s_calib.par_gh1 / 16.0f) + 49.0f;
    float var2 = (((float)s_calib.par_gh2 / 32768.0f) * 0.0005f) + 0.00235f;
    float var3 = (float)s_calib.par_gh3 / 1024.0f;
    float var4 = var1 * (1.0f + (var2 * (float)target_temp));
    float var5 = var4 + (var3 * amb_temp);
    float rhr_factor = 4.0f / (4.0f + (float)s_calib.res_heat_range);
    float rhv_factor = 1.0f / (1.0f + ((float)s_calib.res_heat_val * 0.002f));
    int32_t res = (int32_t)(3.4f * ((var5 * rhr_factor * rhv_factor) - 25.0f));
    if (res < 0) res = 0;
    if (res > 255) res = 255;
    return (uint8_t)res;
}

esp_err_t bme690_init(void)
{
    if (s_dev) return ESP_OK;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME690_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle(), &cfg, &s_dev);
    if (err != ESP_OK) return err;

    // Soft reset
    reg_write(REG_RESET, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Humidity oversampling x1, temp x2, press x16, forced mode trigger later
    reg_write(REG_CTRL_HUM, 0x01);
    // Enable one gas heater profile for a simple VOC/gas-resistance indicator.
    reg_write(REG_CTRL_GAS_0, 0x00);  // heater enabled
    reg_write(REG_GAS_WAIT_0, 0x65);  // ~150 ms heater duration (mult x4, base 0x25=37 → 148ms)
    // RES_HEAT_0 is computed after calibration (needs par_gh* parameters).
    reg_write(REG_CTRL_GAS_1, 0x30);  // run_gas enabled on BME68x variants, profile 0
    // IIR filter coeff 3
    reg_write(REG_CONFIG, 0x10);

    err = read_calibration();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration read: %s", esp_err_to_name(err));
        return err;
    }

    // Now compute heater resistance for 300°C target (typical for VOC sensing).
    // Use 25°C as default ambient until first T reading is available.
    uint8_t res_heat = calc_res_heat(300, 25.0f);
    reg_write(REG_RES_HEAT_0, res_heat);
    ESP_LOGI(TAG, "heater target 300C -> res_heat_0=0x%02X", res_heat);

    err = bsec2_iaq_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BSEC2 IAQ init: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "BME690 init complete");
    return ESP_OK;
}

esp_err_t bme690_self_test(void)
{
    uint8_t id = 0;
    esp_err_t err = reg_read(REG_CHIP_ID, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "self-test FAIL: read err %s", esp_err_to_name(err));
        return err;
    }
    if (id != CHIP_ID_EXPECTED) {
        ESP_LOGE(TAG, "self-test FAIL: chip id 0x%02X (expected 0x%02X)", id, CHIP_ID_EXPECTED);
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(TAG, "self-test PASS (chip id 0x%02X)", id);
    return ESP_OK;
}

esp_err_t bme690_read(bme690_sample_t *out)
{
    if (!out || !s_dev) return ESP_ERR_INVALID_ARG;

    // Re-compute heater setpoint using latest ambient temperature for accuracy.
    float amb = (s_calib.t_fine != 0.0f) ? (s_calib.t_fine / 5120.0f) : 25.0f;
    uint8_t rh = calc_res_heat(300, amb);
    reg_write(REG_RES_HEAT_0, rh);

    // Trigger forced measurement: osrs_t=2(010), osrs_p=5(101), mode=01
    // ctrl_meas = (osrs_t<<5) | (osrs_p<<2) | mode
    esp_err_t err = reg_write(REG_CTRL_MEAS, (2 << 5) | (5 << 2) | 0x01);
    if (err != ESP_OK) return err;

    err = wait_for_forced_measurement();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "measurement wait failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t fields[FIELD_COUNT][FIELD_LENGTH];
    int selected = 0;
    bool found_gas = false;
    for (int i = 0; i < FIELD_COUNT; i++) {
        err = reg_read(REG_FIELD0 + (i * FIELD_STRIDE), fields[i], FIELD_LENGTH);
        if (err != ESP_OK) return err;

        uint8_t *gas = &fields[i][FIELD_GAS_OFF];
        bool frame_new = (fields[i][FIELD_STATUS_OFF] & NEW_DATA_BIT) != 0;
        bool frame_valid = (gas[1] & GAS_VALID_BIT) != 0;
        bool frame_heat = (gas[1] & HEAT_STAB_BIT) != 0;
        if (frame_new && frame_valid && frame_heat) {
            selected = i;
            found_gas = true;
            break;
        }
    }

    uint8_t *raw = fields[selected];
    uint32_t adc_p = ((uint32_t)raw[FIELD_PRESS_OFF] << 12) |
                     ((uint32_t)raw[FIELD_PRESS_OFF + 1] << 4) |
                     (raw[FIELD_PRESS_OFF + 2] >> 4);
    uint32_t adc_t = ((uint32_t)raw[FIELD_TEMP_OFF] << 12) |
                     ((uint32_t)raw[FIELD_TEMP_OFF + 1] << 4) |
                     (raw[FIELD_TEMP_OFF + 2] >> 4);
    uint16_t adc_h = ((uint16_t)raw[FIELD_HUM_OFF] << 8) | raw[FIELD_HUM_OFF + 1];
    uint8_t *gas_raw = &raw[FIELD_GAS_OFF];
    uint16_t adc_gas = ((uint16_t)gas_raw[0] << 2) | (gas_raw[1] >> 6);
    uint8_t gas_range = gas_raw[1] & 0x0F;
    bool gas_valid = (gas_raw[1] & GAS_VALID_BIT) != 0;
    bool heat_stable = (gas_raw[1] & HEAT_STAB_BIT) != 0;

    out->temperature_c = compensate_temperature(adc_t);
    out->pressure_hpa = compensate_pressure(adc_p);
    out->humidity_pct = compensate_humidity(adc_h);
    out->gas_resistance_ohm = compensate_gas_resistance(adc_gas, gas_range);
    static int s_dump = 0;
    if ((s_dump++ & 0x0F) == 0) {
        ESP_LOGI(TAG, "raw adc_t=%u adc_p=%u adc_h=%u → T=%.2fC P=%.2fhPa H=%.1f%%",
                 adc_t, adc_p, adc_h,
                 out->temperature_c, out->pressure_hpa, out->humidity_pct);
        ESP_LOGI(TAG, "calib P1=%u P2=%d P3=%d P4=%d P5=%d P6=%d P7=%d P8=%d P9=%d P10=%u t_fine=%.1f",
                 s_calib.par_p1, s_calib.par_p2, s_calib.par_p3,
                 s_calib.par_p4, s_calib.par_p5, s_calib.par_p6,
                 s_calib.par_p7, s_calib.par_p8, s_calib.par_p9,
                 s_calib.par_p10, s_calib.t_fine);
    }
    if (!found_gas || !gas_valid || !heat_stable || out->gas_resistance_ohm <= 0.0f) {
        ESP_LOGW(TAG, "gas invalid sel=%d f0=%02X/%02X/%02X f1=%02X/%02X/%02X f2=%02X/%02X/%02X adc=%u range=%u valid=%d heat=%d ohm=%.0f",
                 selected,
                 fields[0][FIELD_STATUS_OFF], fields[0][FIELD_GAS_OFF], fields[0][FIELD_GAS_OFF + 1],
                 fields[1][FIELD_STATUS_OFF], fields[1][FIELD_GAS_OFF], fields[1][FIELD_GAS_OFF + 1],
                 fields[2][FIELD_STATUS_OFF], fields[2][FIELD_GAS_OFF], fields[2][FIELD_GAS_OFF + 1],
                 adc_gas, gas_range, gas_valid, heat_stable, out->gas_resistance_ohm);
    } else if (adc_gas == 0 && !s_logged_gas_saturation) {
        ESP_LOGI(TAG, "gas adc saturated low; reporting high resistance %.0fohm", out->gas_resistance_ohm);
        s_logged_gas_saturation = true;
    }
    bsec2_iaq_output_t iaq = { 0 };
    if (bsec2_iaq_process(out->temperature_c, out->humidity_pct, out->pressure_hpa,
                          out->gas_resistance_ohm, &iaq) == ESP_OK && iaq.valid) {
        out->iaq = iaq.iaq;
        out->iaq_accuracy = iaq.accuracy;
        out->iaq_from_bsec2 = iaq.from_bsec2;
    } else {
        out->iaq = 500;
        out->iaq_accuracy = 0;
        out->iaq_from_bsec2 = false;
    }
    return ESP_OK;
}
