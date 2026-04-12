# CopilotDemoPi — Spec Driven Development (SDD)

> This spec defines what the firmware should do. AI generates the code and tests.
> Human writes the spec. AI writes the implementation.

---

## 1. Product Spec

### 1.1 Overview
CopilotDemoPi is a smart Bluetooth speaker with environmental monitoring, running on ESP32-S3-PICO-1-N8R8.

### 1.2 Operating Modes

| Mode | Description | LED Color |
|------|-------------|-----------|
| `MODE_BLUETOOTH` | A2DP audio receiver, phone streams music | 🔵 Blue breathing |
| `MODE_SDCARD` | Play MP3/WAV files from MicroSD | 🟢 Green solid |
| `MODE_RADIO` | HTTP audio stream (internet radio) | 🟡 Yellow breathing |
| `MODE_STANDBY` | LCD on, no audio, sensors active | ⚪ White dim |
| `MODE_SLEEP` | Deep sleep, wake on button press | Off |

### 1.3 Mode Switching
- Short press **OpsKey (GPIO5)** → cycle through modes
- Long press **OpsKey (>3s)** → enter deep sleep
- Press OpsKey while sleeping → wake up to `MODE_STANDBY`

---

## 2. Audio Spec

### 2.1 Bluetooth Audio (A2DP Sink)

```
Input:  Bluetooth A2DP stream from phone
Output: I2S → MAX98357A → Speaker
Format: 44100Hz, 16-bit, stereo (mixed to mono by hardware)
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Discover and pair | Device appears as "CopilotDemoPi" in phone BT settings |
| Auto-reconnect | Reconnects to last paired device within 10s of boot |
| AVRCP metadata | Display track title + artist on LCD |
| Volume control | Rotary encoder adjusts volume 0-100% in steps of 5 |
| Pause/Resume | Encoder push button toggles play/pause |

### 2.2 SD Card Playback

```
Input:  MP3/WAV files from MicroSD (/music/*.mp3, /music/*.wav)
Output: I2S → MAX98357A → Speaker
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| File scanning | Scan /music/ folder on boot, build playlist |
| Sequential play | Play files in alphabetical order |
| Next/Previous | Encoder rotate = next/prev track |
| Format support | MP3 (44.1kHz/16bit), WAV (44.1kHz/16bit) |
| No card | Display "No SD Card" on LCD, skip to next mode |
| Corrupt file | Log error, skip to next file |

---

## 3. Display Spec

### 3.1 LCD (ST7789, 240×240, SPI)

```
Interface: SPI (GPIO6=SCLK, GPIO7=MOSI, GPIO14=DC, GPIO21=RST)
Backlight: PWM on GPIO17, default 80%, adjustable
Framework: LVGL
Color: RGB565 byte-swapped
```

### 3.2 UI Screens

| Screen | Content | When |
|--------|---------|------|
| `SCREEN_BOOT` | Logo + "CopilotDemoPi" + firmware version | Boot (3s) |
| `SCREEN_BT_AUDIO` | Track title, artist, volume bar, BT icon | Bluetooth mode |
| `SCREEN_SD_AUDIO` | Filename, track #/total, progress bar | SD card mode |
| `SCREEN_RADIO` | Station name, stream URL | Radio mode |
| `SCREEN_SENSORS` | Temperature, humidity, pressure, IAQ, IMU | Standby mode |
| `SCREEN_SLEEP` | "Sleeping..." then LCD off after 1s | Entering sleep |

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Boot screen | Display logo within 500ms of power on |
| Refresh rate | Sensor data updates every 1s |
| Backlight timeout | LCD backlight off after 30s idle, any input wakes |
| Smooth transitions | LVGL animation between screens, < 200ms |

---

## 4. Sensor Spec

### 4.1 BME690 (I2C 0x76)

```
Interface: I2C (GPIO3=SDA, GPIO4=SCL, 400kHz)
Measurements: Temperature, Humidity, Pressure, IAQ
IAQ Library: Bosch BSEC2
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Temperature | Read ±0.5°C accuracy, display as "23.5°C" |
| Humidity | Read ±3% RH accuracy, display as "45%" |
| Pressure | Read ±0.5 hPa accuracy, display as "1013.2 hPa" |
| Altitude | Calculate from pressure, display as "4m" |
| IAQ | BSEC2 index 0-500, display with color (green/yellow/red) |
| Sampling | Forced mode, read every 3s |
| Self-test | On boot, verify chip ID = 0x61, report pass/fail |

### 4.2 BMI270 (I2C 0x68)

```
Interface: I2C (GPIO3=SDA, GPIO4=SCL, shared bus with BME690)
Measurements: 3-axis acceleration, 3-axis gyroscope
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Accelerometer | ±4g range, display X/Y/Z in m/s² |
| Gyroscope | ±1000°/s range, display X/Y/Z in °/s |
| Orientation | Calculate pitch/roll from accel data |
| Sampling | 50Hz continuous, display update every 200ms |
| Self-test | On boot, run BMI270 built-in self-test, report pass/fail |
| Interrupt | INT1 (GPIO18) for data-ready (optional) |

---

## 5. Power Spec

### 5.1 Battery Monitoring

```
ADC: GPIO1, voltage divider R31+R32 (100kΩ+100kΩ)
Formula: VBAT = ADC_reading × 2
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Voltage reading | Accurate to ±0.05V (10 sample average) |
| Percentage | Linear map: 3.3V=0%, 4.2V=100% |
| Low battery | Warning on LCD at 10% (3.39V) |
| Critical | Auto deep sleep at 5% (3.345V) |
| Display | Battery icon with 4 levels on status bar |

### 5.2 Deep Sleep

```
Wake source: GPIO5 (OpsKey), RTC GPIO, low-level trigger
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Enter sleep | Long press OpsKey >3s → display "Sleeping..." → LCD off → deep sleep |
| Pre-sleep | Send LCD SLPIN, gpio_reset_pin() all non-wake GPIOs |
| Wake | Press OpsKey → boot → restore last mode |
| Sleep current | < 1.5mA (measured with PPK II) |
| RTC accuracy | 32.768kHz crystal, drift < 5s/day |

---

## 6. User Interface Spec

### 6.1 Rotary Encoder (GPIO12=A, GPIO13=B)

```
Peripheral: PCNT (Pulse Counter) hardware decoding
```

| Context | Rotate CW | Rotate CCW | Push |
|---------|-----------|------------|------|
| Audio playing | Volume +5% | Volume -5% | Play/Pause |
| Audio stopped | Next track | Prev track | Play |
| Sensor screen | Scroll data | Scroll data | Switch display |
| Any | — | — | — |

### 6.2 Buttons

| Button | Short Press | Long Press (>3s) |
|--------|------------|------------------|
| OpsKey (GPIO5) | Cycle mode | Enter deep sleep |
| Boot (GPIO0) | — | Enter download mode (with reset) |

### 6.3 LEDs

| LED | GPIO | Driver | Function |
|-----|------|--------|----------|
| WS2812B (D3) | GPIO8 | RMT | Mode indicator (color = current mode) |
| Red (D6) | GPIO2 | LEDC PWM | Demo / alert |
| Green (D7) | GPIO33 | LEDC PWM | Demo / status |
| Blue (D8) | GPIO34 | LEDC PWM | Demo / BT active |

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Boot test | Cycle R→G→B on boot (500ms each) |
| Mode color | WS2812B shows mode color (see Mode table) |
| BT connected | Blue LED solid when BT device connected |
| Charging | Red LED on when USB charging (from BQ24072 ~CHG) |

---

## 7. Storage Spec

### 7.1 MicroSD (SPI Mode)

```
Interface: SPI (GPIO35=MOSI, GPIO36=CLK, GPIO37=CS, GPIO38=MISO)
Filesystem: FAT32
Mount point: /sdcard
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Mount | Auto-mount on boot if card present |
| No card | Graceful handling, display "No SD Card" |
| Hot plug | Detect card removal, unmount safely |
| Max size | Support up to 32GB |
| File listing | List files in /music/ directory |

---

## 8. Connectivity Spec

### 8.1 WiFi

```
Mode: STA (Station)
Provisioning: BLE provisioning or AP+Web portal
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Provisioning | First boot → BLE provisioning mode |
| Auto-connect | Reconnect to saved WiFi on boot |
| Status | WiFi icon on LCD status bar |
| Fallback | If WiFi fails, continue in offline modes |

### 8.2 Bluetooth

```
Profiles: A2DP Sink, AVRCP
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Discoverable | Name: "CopilotDemoPi" |
| Pairing | Simple pairing, no PIN |
| Codec | SBC (required), AAC (optional) |
| Range | ~10m typical |

---

## 9. Boot Sequence Spec

```
Power on
  │
  ├── 1. Hardware init (GPIO, SPI, I2C, I2S) .......... <100ms
  ├── 2. LCD init → show boot logo ..................... <500ms
  ├── 3. LED test (R→G→B cycle) ....................... 1500ms
  ├── 4. Sensor self-test (BME690 + BMI270) ........... <200ms
  ├── 5. MicroSD mount (if present) ................... <500ms
  ├── 6. WiFi connect (background, non-blocking) ...... async
  ├── 7. Bluetooth init (A2DP sink) ................... <1000ms
  ├── 8. Enter last saved mode ........................ immediate
  │
  Total boot time target: < 3 seconds to usable
```

| Requirement | Acceptance Criteria |
|-------------|-------------------|
| Boot time | Logo visible < 500ms, usable < 3s |
| Self-test fail | Show error on LCD, continue with available hardware |
| First boot | Enter BLE provisioning if no saved WiFi |

---

## 10. Error Handling Spec

| Error | Behavior |
|-------|----------|
| BME690 not responding | Display "Sensor Error", skip sensor readings |
| BMI270 not responding | Display "IMU Error", skip IMU readings |
| SD card corrupt | Display "SD Error", skip SD mode |
| WiFi connect fail | Retry 3 times, then fallback to offline |
| BT disconnect | Auto-reconnect for 30s, then idle |
| Low battery | Warning → auto sleep at critical |
| Watchdog timeout | Auto reboot |

---

## 11. Test Criteria

Every spec item above maps to a test:

```
test_bluetooth_discovery()     → Phone can find "CopilotDemoPi"
test_bluetooth_audio()         → Play audio, verify I2S output
test_volume_control()          → Encoder changes volume 0-100%
test_sd_card_playback()        → Play MP3 from SD card
test_bme690_self_test()        → Chip ID = 0x61
test_bme690_temperature()      → Read temp, verify ±0.5°C
test_bmi270_self_test()        → Built-in self-test passes
test_battery_reading()         → ADC reads VBAT ±0.05V
test_deep_sleep_current()      → PPK II measures < 1.5mA
test_boot_time()               → Logo < 500ms, usable < 3s
test_mode_switching()          → OpsKey cycles all modes
test_lcd_display()             → Each screen renders correctly
test_led_boot_test()           → R→G→B cycle on boot
test_encoder_rotation()        → CW = +, CCW = -
test_wifi_provisioning()       → BLE provisioning flow works
test_error_handling()          → Each error shows correct message
```

---

*CopilotDemoPi SDD v1.0*
*Spec by Cysoft & Chac 🐕 | April 2026*

*Human writes the spec. AI writes the code. Tests verify the spec.*
