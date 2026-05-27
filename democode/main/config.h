#pragma once

// =============================================================
// CopilotDemoPi — Hardware pin map and constants
// Authoritative pin map (resolves gaps in hw_spec.md)
// =============================================================

// ---------- SPI LCD (ST7789, dedicated SPI2_HOST) ----------
#define LCD_SCLK_GPIO       6
#define LCD_MOSI_GPIO       7
#define LCD_DC_GPIO         14
#define LCD_RST_GPIO        21
#define LCD_BL_GPIO         17
// J5 pin 8 is wired to GND, which permanently asserts the module's CS pin.
// No CS GPIO is needed; the panel is always selected.
#define LCD_CS_GPIO         (-1)
// Some 1.54" ST7789 modules need invert_color=true (IPS), others false.
// Flip this if the screen comes up as a photographic negative.
#define LCD_INVERT_COLOR    true
#define LCD_WIDTH           240
#define LCD_HEIGHT          240
#define LCD_DIAGNOSTIC_LOOP 0     // 1: stop boot in repeating LCD bring-up pattern

// ---------- I2S Audio (MAX98357A) ----------
#define I2S_BCLK_GPIO       9
#define I2S_LRCLK_GPIO      10
#define I2S_DOUT_GPIO       11
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_BITS          16
#define AUDIO_CHANNELS      2

// ---------- I2C Bus (BME690 + BMI270) ----------
#define I2C_SDA_GPIO        3
#define I2C_SCL_GPIO        4
#define I2C_FREQ_HZ         400000
#define BME690_I2C_ADDR     0x76
#define BMI270_I2C_ADDR     0x68

// ---------- MicroSD (SPI mode, SPI3_HOST) ----------
// !!! HARDWARE CONFLICT: GPIO 35-37 are reserved for octal PSRAM on N8R8.
// MicroSD is therefore unusable on this board revision while octal PSRAM
// is enabled. Pins are kept for reference only; sdcard driver is gated off.
#define SD_MOSI_GPIO        35   // UNUSABLE on N8R8 (octal PSRAM SPIIO6)
#define SD_CLK_GPIO         36   // UNUSABLE on N8R8 (octal PSRAM SPIIO7)
#define SD_CS_GPIO          37   // UNUSABLE on N8R8 (octal PSRAM SPIDQS)
#define SD_MISO_GPIO        38
#define SD_MOUNT_POINT      "/sdcard"

// ---------- User Interface ----------
#define BTN_BOOT_GPIO       0
#define BTN_OPSKEY_GPIO     5
#define ENCODER_A_GPIO      12
#define ENCODER_B_GPIO      13

// ---------- LEDs ----------
#define WS2812B_GPIO        8

// External WS2812 strip on extension header (M5Stack 30-LED, 20cm).
// Default to GPIO45; if it does not light, swap to GPIO46.
#define WS2812_STRIP_GPIO   46
#define WS2812_STRIP_COUNT  30

#define LED_RED_GPIO        2
// !!! HARDWARE CONFLICT: GPIO 33-37 are reserved for octal PSRAM on N8R8.
// LED_GREEN (GPIO33), LED_BLUE (GPIO34) and the entire MicroSD bus (35-38)
// CANNOT be used while octal PSRAM is enabled. Driving these pins corrupts
// PSRAM accesses and triggers an interrupt-watchdog reset.
// They are kept here for documentation but disabled at runtime.
#define LED_GREEN_GPIO      33   // UNUSABLE on N8R8 (octal PSRAM SPIIO4)
#define LED_BLUE_GPIO       34   // UNUSABLE on N8R8 (octal PSRAM SPIIO5)

// ---------- IMU Interrupt ----------
#define IMU_INT1_GPIO       18

// ---------- Battery ADC ----------
#define BATT_ADC_GPIO       1     // ADC1_CH0
#define BATT_DIVIDER_RATIO  2.0f  // R31+R32

// ---------- USB (reserved) ----------
#define USB_DM_GPIO         19
#define USB_DP_GPIO         20

// ---------- Battery thresholds (V) ----------
#define BATT_VOLTAGE_FULL   4.20f
#define BATT_VOLTAGE_LOW    3.50f
#define BATT_VOLTAGE_SHUT   3.30f

// ---------- Device identity ----------
#define DEVICE_NAME         "CopilotDemoPi"
#define FIRMWARE_VERSION    "0.1.0"
