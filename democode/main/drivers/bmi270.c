// BMI270 driver — uploads Bosch init image, then reads accel/gyro.
#include "bmi270.h"
#include "bmi270_config.h"
#include "i2c_bus.h"
#include "config.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmi270";

#define REG_CHIP_ID         0x00
#define REG_ERR             0x02
#define REG_STATUS          0x03
#define REG_ACC_X_LSB       0x0C
#define REG_GYR_X_LSB       0x12
#define REG_EVENT           0x1B
#define REG_INT_STATUS_0    0x1C
#define REG_INT_STATUS_1    0x1D
#define REG_INTERNAL_STATUS 0x21
#define REG_ACC_CONF        0x40
#define REG_ACC_RANGE       0x41
#define REG_GYR_CONF        0x42
#define REG_GYR_RANGE       0x43
#define REG_INIT_CTRL       0x59
#define REG_INIT_ADDR_0     0x5B
#define REG_INIT_ADDR_1     0x5C
#define REG_INIT_DATA       0x5E
#define REG_PWR_CONF        0x7C
#define REG_PWR_CTRL        0x7D
#define REG_CMD             0x7E

#define CMD_SOFT_RESET      0xB6

#define STATUS_DRDY_ACC     0x80
#define STATUS_DRDY_GYR     0x40

#define INTERNAL_STATUS_INIT_OK 0x01
#define INTERNAL_STATUS_MASK    0x0F

#define PWR_CTRL_ACC_EN     0x04
#define PWR_CTRL_GYR_EN     0x02

// 100Hz | OSR2 | filter perf
#define ACC_CONF_100HZ_OSR2_PERF 0xA8
#define ACC_RANGE_4G        0x01
#define ACC_LSB_PER_G_4G    8192.0f

// 100Hz | OSR2 | filter perf | noise perf
#define GYR_CONF_100HZ_OSR2_PERF 0xE9
#define GYR_RANGE_1000DPS   0x01
#define GYR_LSB_PER_DPS_1000 32.768f

#define CHIP_ID_EXPECTED    0x24
#define CONFIG_CHUNK_BYTES  64

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_dumped_read_debug = false;

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 100);
}

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 100);
}

static esp_err_t reg_write_burst(uint8_t reg, const uint8_t *data, size_t n)
{
    uint8_t buf[1 + CONFIG_CHUNK_BYTES];
    if (n > CONFIG_CHUNK_BYTES) return ESP_ERR_INVALID_SIZE;
    buf[0] = reg;
    for (size_t i = 0; i < n; i++) buf[1 + i] = data[i];
    return i2c_master_transmit(s_dev, buf, n + 1, 200);
}

static uint8_t reg_read_u8(uint8_t reg)
{
    uint8_t v = 0xFF;
    (void)reg_read(reg, &v, 1);
    return v;
}

static void bmi270_dump_registers(const char *reason)
{
    if (!s_dev) return;
    uint8_t raw_acc[6] = { 0 };
    uint8_t raw_gyr[6] = { 0 };
    esp_err_t acc_err = reg_read(REG_ACC_X_LSB, raw_acc, sizeof(raw_acc));
    esp_err_t gyr_err = reg_read(REG_GYR_X_LSB, raw_gyr, sizeof(raw_gyr));

    ESP_LOGW(TAG,
             "%s: id=0x%02X err=0x%02X status=0x%02X event=0x%02X int0=0x%02X int1=0x%02X internal=0x%02X",
             reason,
             reg_read_u8(REG_CHIP_ID),
             reg_read_u8(REG_ERR),
             reg_read_u8(REG_STATUS),
             reg_read_u8(REG_EVENT),
             reg_read_u8(REG_INT_STATUS_0),
             reg_read_u8(REG_INT_STATUS_1),
             reg_read_u8(REG_INTERNAL_STATUS));
    ESP_LOGW(TAG,
             "%s: pwr_conf=0x%02X pwr_ctrl=0x%02X acc_conf=0x%02X acc_range=0x%02X gyr_conf=0x%02X gyr_range=0x%02X",
             reason,
             reg_read_u8(REG_PWR_CONF),
             reg_read_u8(REG_PWR_CTRL),
             reg_read_u8(REG_ACC_CONF),
             reg_read_u8(REG_ACC_RANGE),
             reg_read_u8(REG_GYR_CONF),
             reg_read_u8(REG_GYR_RANGE));
    ESP_LOGW(TAG,
             "%s: raw_acc(%s)=%02X %02X %02X %02X %02X %02X raw_gyr(%s)=%02X %02X %02X %02X %02X %02X",
             reason,
             esp_err_to_name(acc_err),
             raw_acc[0], raw_acc[1], raw_acc[2], raw_acc[3], raw_acc[4], raw_acc[5],
             esp_err_to_name(gyr_err),
             raw_gyr[0], raw_gyr[1], raw_gyr[2], raw_gyr[3], raw_gyr[4], raw_gyr[5]);
}

static esp_err_t bmi270_upload_config(void)
{
    const size_t total = sizeof(s_bmi270_config_file);
    ESP_LOGI(TAG, "uploading %u-byte config", (unsigned)total);

    // Disable config loading while writing init data.
    esp_err_t err = reg_write(REG_INIT_CTRL, 0x00);
    if (err != ESP_OK) { ESP_LOGE(TAG, "init_ctrl=0 failed: %s", esp_err_to_name(err)); return err; }

    for (size_t i = 0; i < total; i += CONFIG_CHUNK_BYTES) {
        size_t chunk = (total - i) < CONFIG_CHUNK_BYTES ? (total - i) : CONFIG_CHUNK_BYTES;

        // Index is 16-bit word offset (byte offset / 2).
        uint16_t word_idx = (uint16_t)(i / 2);
        uint8_t addr_pkt[3] = {
            REG_INIT_ADDR_0,
            (uint8_t)(word_idx & 0x0F),
            (uint8_t)((word_idx >> 4) & 0xFF),
        };
        err = i2c_master_transmit(s_dev, addr_pkt, sizeof(addr_pkt), 100);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "init_addr write failed @ %u: %s", (unsigned)i, esp_err_to_name(err));
            return err;
        }

        err = reg_write_burst(REG_INIT_DATA, &s_bmi270_config_file[i], chunk);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "init_data burst failed @ %u: %s", (unsigned)i, esp_err_to_name(err));
            return err;
        }
    }

    // Trigger config load.
    err = reg_write(REG_INIT_CTRL, 0x01);
    if (err != ESP_OK) { ESP_LOGE(TAG, "init_ctrl=1 failed: %s", esp_err_to_name(err)); return err; }

    // Bosch: poll up to 150ms for INTERNAL_STATUS == BMI2_INIT_OK.
    uint8_t internal = 0;
    for (int t = 0; t < 30; t++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        internal = reg_read_u8(REG_INTERNAL_STATUS);
        if ((internal & INTERNAL_STATUS_MASK) == INTERNAL_STATUS_INIT_OK) {
            ESP_LOGI(TAG, "config loaded OK (internal=0x%02X) after %d ms", internal, (t + 1) * 10);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "config load FAILED, internal_status=0x%02X", internal);
    return ESP_ERR_TIMEOUT;
}

esp_err_t bmi270_init(void)
{
    if (s_dev) return ESP_OK;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI270_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_handle(), &cfg, &s_dev);
    if (err != ESP_OK) return err;

    uint8_t id = 0;
    (void)reg_read(REG_CHIP_ID, &id, 1);
    ESP_LOGI(TAG, "chip id pre-reset: 0x%02X", id);

    // Soft reset (write 0xB6 to CMD register).
    (void)reg_write(REG_CMD, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Dummy read after reset to ensure I2C interface stays selected.
    (void)reg_read(REG_CHIP_ID, &id, 1);

    // Disable advanced power save before uploading the init image.
    err = reg_write(REG_PWR_CONF, 0x00);
    if (err != ESP_OK) return err;
    esp_rom_delay_us(450);

    err = bmi270_upload_config();
    if (err != ESP_OK) {
        bmi270_dump_registers("init load failed");
        return err;
    }

    // Configure accel: 100Hz, perf+osr2, ±4g.
    err = reg_write(REG_ACC_CONF, ACC_CONF_100HZ_OSR2_PERF);
    if (err != ESP_OK) return err;
    err = reg_write(REG_ACC_RANGE, ACC_RANGE_4G);
    if (err != ESP_OK) return err;

    // Configure gyro: 100Hz, perf+noise perf, ±1000dps.
    err = reg_write(REG_GYR_CONF, GYR_CONF_100HZ_OSR2_PERF);
    if (err != ESP_OK) return err;
    err = reg_write(REG_GYR_RANGE, GYR_RANGE_1000DPS);
    if (err != ESP_OK) return err;

    // Enable accel + gyro after config + init load.
    err = reg_write(REG_PWR_CTRL, PWR_CTRL_ACC_EN | PWR_CTRL_GYR_EN);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(50));
    bmi270_dump_registers("init dump");
    return ESP_OK;
}

esp_err_t bmi270_self_test(void)
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

esp_err_t bmi270_read(bmi270_sample_t *out)
{
    if (!out || !s_dev) return ESP_ERR_INVALID_ARG;
    uint8_t raw_acc[6];
    uint8_t raw_gyr[6];
    uint8_t status = 0;

    esp_err_t err = reg_read(REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        if (!s_dumped_read_debug) {
            s_dumped_read_debug = true;
            bmi270_dump_registers("status read failed");
        }
        return err;
    }

    err = reg_read(REG_ACC_X_LSB, raw_acc, sizeof(raw_acc));
    if (err != ESP_OK) return err;

    err = reg_read(REG_GYR_X_LSB, raw_gyr, sizeof(raw_gyr));
    if (err != ESP_OK) return err;

    int16_t ax = (int16_t)((raw_acc[1] << 8) | raw_acc[0]);
    int16_t ay = (int16_t)((raw_acc[3] << 8) | raw_acc[2]);
    int16_t az = (int16_t)((raw_acc[5] << 8) | raw_acc[4]);
    int16_t gx = (int16_t)((raw_gyr[1] << 8) | raw_gyr[0]);
    int16_t gy = (int16_t)((raw_gyr[3] << 8) | raw_gyr[2]);
    int16_t gz = (int16_t)((raw_gyr[5] << 8) | raw_gyr[4]);

    if ((status & (STATUS_DRDY_ACC | STATUS_DRDY_GYR)) == 0) {
        if (!s_dumped_read_debug) {
            s_dumped_read_debug = true;
            ESP_LOGW(TAG, "data not ready: status=0x%02X", status);
            bmi270_dump_registers("not ready dump");
        }
    }

    out->ax_g = ax / ACC_LSB_PER_G_4G;
    out->ay_g = ay / ACC_LSB_PER_G_4G;
    out->az_g = az / ACC_LSB_PER_G_4G;
    out->gx_dps = gx / GYR_LSB_PER_DPS_1000;
    out->gy_dps = gy / GYR_LSB_PER_DPS_1000;
    out->gz_dps = gz / GYR_LSB_PER_DPS_1000;
    return ESP_OK;
}
