#include "sdcard.h"
#include "config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

static const char *TAG = "sdcard";
#define SD_HOST SPI3_HOST

static sdmmc_card_t *s_card = NULL;

// !!! HARDWARE CONFLICT on ESP32-S3-PICO-1-N8R8:
// GPIO 35/36/37 are reserved for the octal-PSRAM data bus (SPIIO6, SPIIO7,
// SPIDQS). Configuring them as a SPI master corrupts PSRAM access and
// triggers an interrupt-watchdog reset (TG1WDT_SYS_RST). The MicroSD slot
// is therefore not usable on this PCB revision. The driver intentionally
// returns ESP_ERR_NOT_SUPPORTED until the hardware is reworked or PSRAM
// is moved to quad mode.
esp_err_t sdcard_init(void)
{
    ESP_LOGI(TAG, "MicroSD disabled: GPIO 35/36/37 conflict with octal PSRAM (N8R8)");
    s_card = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

bool sdcard_is_mounted(void) { return s_card != NULL; }

#if 0  // Original SPI3 mount path — re-enable only after HW fix.
esp_err_t sdcard_init_real(void)
{
    if (s_card) return ESP_OK;

    spi_bus_config_t buscfg = {
        .mosi_io_num = SD_MOSI_GPIO,
        .miso_io_num = SD_MISO_GPIO,
        .sclk_io_num = SD_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t err = spi_bus_initialize(SD_HOST, &buscfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi init: %s", esp_err_to_name(err));
        return err;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_HOST;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = SD_CS_GPIO;
    slot.host_id = SD_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no SD card or mount failed: %s", esp_err_to_name(err));
        s_card = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "SD mounted at %s (%lluMB)", SD_MOUNT_POINT,
             ((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) / (1024 * 1024));
    return ESP_OK;
}
#endif
