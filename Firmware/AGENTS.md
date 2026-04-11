# AGENTS.md — CopilotDemoPi Firmware

## Project Overview

This is the firmware for **CopilotDemoPi**, a smart Bluetooth speaker with environmental monitoring, built on ESP32-S3-PICO-1-N8R8 running ESP-IDF.

Hardware spec: see `CopilotDemoPi_hardware_resource.md`

---

## Core Rules

### 1. Write Your Own Drivers — No External Libraries

- **Write all peripheral drivers from scratch** using ESP-IDF APIs only
- **Do NOT use** third-party Arduino libraries, PlatformIO libraries, or GitHub repos
- Allowed dependencies:
  - **ESP-IDF components only** (`driver/`, `esp_adc/`, `hal/`, `spi_master`, `i2c_master`)
  - **LVGL** for UI rendering
  - **Bosch BSEC2** for BME690 IAQ (closed-source, no alternative)
  - **ESP-ADF** (Audio Development Framework) for Bluetooth A2DP sink
- If you need a driver for BME690, BMI270, ST7789, MAX98357A, MicroSD — **write it yourself** based on the datasheet

### 2. Language & Framework

- Language: **C** (not C++)
- Framework: **ESP-IDF v5.x**
- Build system: **CMake** (ESP-IDF standard)
- No Arduino.h, no Wire.h, no Adafruit libraries

### 3. Code Style

- Naming: `snake_case` for functions and variables
- Constants: `UPPER_SNAKE_CASE`
- Prefix all module functions with module name: `lcd_init()`, `bme690_read()`, `audio_play()`
- File naming: `lcd.c / lcd.h`, `bme690.c / bme690.h`, `audio.c / audio.h`
- Every `.c` file has a corresponding `.h` with include guard
- Use `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()` for logging — **never printf()**
- Log tag = filename: `static const char *TAG = "lcd";`

### 4. Error Handling

- Every hardware function returns `esp_err_t`
- Check every return value — no silent failures
- Use `ESP_ERROR_CHECK()` for init-time calls
- Use `ESP_RETURN_ON_ERROR()` for runtime calls
- Log errors with `ESP_LOGE()` before returning

### 5. Project Structure

```
demopi-firmware/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                  ← Entry point, app_main()
│   ├── config.h                ← Pin definitions, I2C addresses, constants
│   ├── drivers/
│   │   ├── i2c_bus.c / .h      ← I2C bus init and helpers
│   │   ├── bme690.c / .h       ← BME690 temp/humidity/pressure/IAQ driver
│   │   ├── bmi270.c / .h       ← BMI270 6-axis IMU driver
│   │   ├── lcd.c / .h          ← ST7789 SPI LCD driver
│   │   ├── max98357a.c / .h    ← I2S audio output driver
│   │   ├── ws2812b.c / .h      ← WS2812B LED driver (RMT peripheral)
│   │   ├── encoder.c / .h      ← Rotary encoder driver (PCNT peripheral)
│   │   ├── sdcard.c / .h       ← MicroSD card driver (SPI mode)
│   │   ├── battery.c / .h      ← Battery voltage reading (internal ADC)
│   │   └── led_pwm.c / .h      ← PWM LED control (LEDC peripheral)
│   ├── app/
│   │   ├── bt_audio.c / .h     ← Bluetooth A2DP sink (music receiver)
│   │   ├── display.c / .h      ← UI rendering (uses lcd.h + LVGL)
│   │   ├── env_monitor.c / .h  ← Environmental monitoring (BME690 IAQ)
│   │   ├── motion.c / .h       ← IMU data processing
│   │   └── power.c / .h        ← Power management, deep sleep, low battery
│   └── assets/
│       └── logo_rgb565.h       ← Boot logo (240x240 RGB565 byte-swapped array)
```

### 6. Hardware Constants — config.h

```c
#pragma once

// ============================================
// SPI LCD (ST7789, dedicated bus)
// ============================================
#define LCD_SCLK_GPIO       6
#define LCD_MOSI_GPIO       7
#define LCD_DC_GPIO         14
#define LCD_RST_GPIO        21
#define LCD_BL_GPIO         17
#define LCD_WIDTH           240
#define LCD_HEIGHT          240

// ============================================
// I2S Audio (MAX98357A)
// ============================================
#define I2S_BCLK_GPIO       9
#define I2S_LRCLK_GPIO      10
#define I2S_DOUT_GPIO       11

// ============================================
// I2C Bus (Sensors: BME690 + BMI270)
// ============================================
#define I2C_SDA_GPIO        3
#define I2C_SCL_GPIO        4
#define I2C_FREQ_HZ         400000
#define BME690_I2C_ADDR     0x76
#define BMI270_I2C_ADDR     0x68

// ============================================
// MicroSD (SPI mode)
// ============================================
#define SD_MOSI_GPIO        35
#define SD_CLK_GPIO         36
#define SD_CS_GPIO          37
#define SD_MISO_GPIO        38

// ============================================
// User Interface
// ============================================
#define BTN_BOOT_GPIO       0     // Boot button
#define BTN_OPSKEY_GPIO     5     // User button, deep sleep wake
#define ENCODER_A_GPIO      12
#define ENCODER_B_GPIO      13

// ============================================
// LEDs
// ============================================
#define WS2812B_GPIO        8     // RGB status LED
#define LED_RED_GPIO        2     // PWM demo LED
#define LED_GREEN_GPIO      33    // PWM demo LED
#define LED_BLUE_GPIO       34    // PWM demo LED

// ============================================
// IMU Interrupt
// ============================================
#define IMU_INT1_GPIO       18

// ============================================
// Battery ADC
// ============================================
#define BATT_ADC_GPIO       1
#define BATT_DIVIDER_RATIO  2.0   // R31+R32 voltage divider

// ============================================
// USB (reserved, do not reassign)
// ============================================
#define USB_DM_GPIO         19
#define USB_DP_GPIO         20

// ============================================
// Battery thresholds
// ============================================
#define BATT_VOLTAGE_FULL   4.20
#define BATT_VOLTAGE_LOW    3.50
#define BATT_VOLTAGE_SHUT   3.30
```

### 7. Bus Architecture — No Contention

SPI (LCD) and I2C (sensors) are on **separate, dedicated GPIO pins**. No bus sharing, no mutex needed.

```
SPI Bus (LCD only):       GPIO6 (SCLK) + GPIO7 (MOSI) + GPIO14 (DC) + GPIO21 (RST)
I2C Bus (Sensors only):   GPIO3 (SDA) + GPIO4 (SCL)
I2S Bus (Audio only):     GPIO9 (BCLK) + GPIO10 (LRCLK) + GPIO11 (DIN)
SPI Bus (SD card only):   GPIO35 (MOSI) + GPIO36 (CLK) + GPIO37 (CS) + GPIO38 (MISO)
```

All four buses are independent. No contention, no mutex required between them.

### 8. Driver Implementation Guidelines

**I2C Bus (GPIO3/GPIO4):**
- Use ESP-IDF `i2c_master` (new driver, not legacy)
- Init once in `i2c_bus_init()`, share handle across BME690 and BMI270
- 400kHz Fast mode

**BME690:**
- Forced mode (not continuous) — read on demand, save power
- Read all four: temperature, humidity, pressure, gas resistance
- IAQ calculation via BSEC2 library (binary blob, link as static lib)
- Temperature compensation required before humidity/pressure

**BMI270:**
- Configure accel ±4g, gyro ±1000°/s for general use
- Use INT1 (GPIO18) for data-ready interrupt
- Support both polling and interrupt modes

**ST7789 LCD:**
- Use ESP-IDF `spi_master` driver
- Init sequence from ST7789V datasheet
- Color format: **RGB565 byte-swapped** (confirmed from Chac 2.2)
- Support: fill rect, draw pixel, draw bitmap, draw text
- Backlight PWM via LEDC peripheral on GPIO17

**MAX98357A:**
- Use ESP-IDF `i2s_std` driver
- Configure: 44100Hz sample rate, 16-bit, stereo (chip mixes to mono)
- SD_MODE controlled by hardware (1MΩ to VDD = L+R mix)

**WS2812B:**
- Use ESP-IDF **RMT peripheral** — hardware-timed, no CPU jitter
- Single LED, but write driver to support N LEDs

**Rotary Encoder:**
- Use ESP-IDF **PCNT (Pulse Counter)** peripheral — hardware decoding
- No software debouncing needed with PCNT

**MicroSD:**
- SPI mode via `spi_master`
- Use ESP-IDF VFS + FATFS for file access
- Mount at `/sdcard`

**PWM LEDs:**
- Use ESP-IDF **LEDC** peripheral
- 3 channels for Red/Green/Blue demo LEDs
- 1 channel for LCD backlight

**Battery:**
- Use ESP-IDF `esp_adc` oneshot driver
- Read GPIO1 (ADC1_CH0)
- Apply voltage divider ratio (×2)
- Average 10 samples for stability

### 9. Bluetooth Audio

- Use **ESP-ADF** A2DP Sink component
- ESP32-S3 receives Bluetooth audio from phone
- Decode SBC/AAC → PCM → I2S → MAX98357A → Speaker
- Display track info on LCD via AVRCP metadata

### 10. Memory & Performance

- ESP32-S3 has 8MB PSRAM — use it for audio buffers and LVGL frame buffer
- LCD frame buffer in PSRAM: 240×240×2 = 115KB (affordable with 8MB)
- Audio ring buffer: 32KB in PSRAM
- Target main loop: < 100ms per iteration (10 Hz sensor refresh)

### 11. Power Management

- Deep sleep on long OpsKey press (GPIO5)
- Wake on OpsKey press (GPIO5, RTC wake source)
- LCD backlight off after 30s idle (configurable via encoder)
- Shutdown at VBAT < 3.3V — display warning first, then deep sleep
- Before deep sleep: `gpio_reset_pin()` all non-wakeup GPIOs

### 12. Testing

- Every driver must have a basic self-test: `bme690_self_test()`, `bmi270_self_test()`
- Self-test runs at boot, result shown on LCD
- If a sensor fails, display error and continue with available sensors
- LED test: cycle R/G/B on boot
- Audio test: play startup chime

### 13. Do NOT

- ❌ Use Arduino framework or libraries
- ❌ Include third-party GitHub repos via git submodule (except LVGL, BSEC2, ESP-ADF)
- ❌ Use `printf()` — use ESP log macros
- ❌ Ignore error return values
- ❌ Use blocking delays in main loop (`vTaskDelay` is OK in separate tasks)
- ❌ Hard-code magic numbers — use `config.h` constants
- ❌ Assign GPIO19 or GPIO20 to anything other than USB

---

*This file defines the rules for AI coding agents working on this project.*
*All code must comply. Non-compliant code will be rejected at review.*
