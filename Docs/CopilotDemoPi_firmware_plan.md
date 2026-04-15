# CopilotDemoPi — Firmware Development Plan

> From bare PCB to working product
> ESP-IDF v5.x | C | LVGL | ESP-ADF
> by Cysoft & Chac 🐕 | April 2026

---

## Overview

CopilotDemoPi firmware is developed in three phases: hardware validation, audio features, and integrated UI. Each phase builds on the previous, with clear test criteria before proceeding.

**Development approach:** AI-assisted (Codex generates drivers from AGENTS.md + SDD specs, engineer reviews and tests on real hardware).

**Key documents:**
- `AGENTS.md` — coding rules and constraints for AI
- `CopilotDemoPi_SDD.md` — functional specifications and acceptance criteria
- `CopilotDemoPi_hardware_resource.md` — GPIO map, bus architecture, power system

---

## Phase 1: Hardware Bringup (Day 1-3 after board arrives)

*Goal: Verify every hardware subsystem works. One driver at a time.*

### 1.1 Boot & USB (Priority: CRITICAL)

| Task | Description | Pass Criteria |
|------|-------------|---------------|
| Flash firmware | USB-C → esptool flash | "Hello World" on serial monitor |
| USB Serial/JTAG | Console log output | ESP_LOGI messages visible |
| Boot button | GPIO0 + Reset → download mode | Can re-flash at will |

**If this fails, nothing else matters.** Debug: check USB-C wiring, ESP32 power, crystal.

### 1.2 LED Test (Priority: HIGH)

| Task | Driver | GPIO | Pass Criteria |
|------|--------|------|---------------|
| WS2812B RGB | `ws2812b.c` (RMT) | GPIO8 | Red → Green → Blue cycle |
| PWM Red | `led_pwm.c` (LEDC) | GPIO2 | Breathing effect |
| PWM Green | `led_pwm.c` (LEDC) | GPIO33 | Breathing effect |
| PWM Blue | `led_pwm.c` (LEDC) | GPIO34 | Breathing effect |
| Charging LED | Hardware (BQ24072) | — | Red ON when USB charging |
| Power Good LED | Hardware (BQ24072) | — | Green ON when USB connected |

### 1.3 LCD Display (Priority: HIGH)

| Task | Driver | Interface | Pass Criteria |
|------|--------|-----------|---------------|
| SPI init | `lcd.c` (spi_master) | GPIO6(SCLK), GPIO7(MOSI) | No SPI errors |
| LCD init | `lcd.c` | GPIO14(DC), GPIO21(RST) | Screen clears to black |
| Backlight | `lcd.c` (LEDC PWM) | GPIO17 → Q2 | Backlight on/off, brightness adjustable |
| Draw test | `lcd.c` | — | Color bars / test pattern |
| Boot logo | `lcd.c` + `logo_rgb565.h` | — | Chac logo displayed correctly |
| Color format | — | — | Verify RGB565 byte-swapped (from Chac 2.2 experience) |

### 1.4 I2C Sensors (Priority: HIGH)

| Task | Driver | Address | Pass Criteria |
|------|--------|---------|---------------|
| I2C bus init | `i2c_bus.c` (i2c_master) | GPIO3(SDA), GPIO4(SCL) | Bus scan finds 0x76 + 0x68 |
| BME690 ID | `bme690.c` | 0x76 | Chip ID = 0x61 |
| BME690 read | `bme690.c` | 0x76 | Temp ±5°C of room temp, humidity ±20% |
| BME690 BSEC | `bme690.c` + BSEC2 lib | 0x76 | IAQ index 0-500 (may need 24h calibration) |
| BMI270 ID | `bmi270.c` | 0x68 | Chip ID = 0x24 |
| BMI270 self-test | `bmi270.c` | 0x68 | Built-in self-test passes |
| BMI270 read | `bmi270.c` | 0x68 | Accel Z ≈ 9.8 m/s² when flat |

### 1.5 User Input (Priority: MEDIUM)

| Task | Driver | GPIO | Pass Criteria |
|------|--------|------|---------------|
| OpsKey button | GPIO interrupt | GPIO5 | Press detected, serial log |
| Boot button | GPIO interrupt | GPIO0 | Press detected |
| Rotary encoder | `encoder.c` (PCNT) | GPIO12(A), GPIO13(B) | CW = +1, CCW = -1, push = press |

### 1.6 Battery & Power (Priority: MEDIUM)

| Task | Driver | GPIO | Pass Criteria |
|------|--------|------|---------------|
| Battery ADC | `battery.c` (esp_adc) | GPIO1 | Read voltage ±0.1V of multimeter |
| Percentage | `battery.c` | — | 3.3V=0%, 4.2V=100% |
| Charging detect | Read BQ24072 LEDs | D1, D2 | Detect USB plugged / charging / full |

### 1.7 MicroSD Card (Priority: MEDIUM)

| Task | Driver | Interface | Pass Criteria |
|------|--------|-----------|---------------|
| SPI init | `sdcard.c` (spi_master) | GPIO35-38 | No SPI errors |
| Mount FAT32 | `sdcard.c` (VFS+FATFS) | — | Mount at /sdcard |
| Read file | `sdcard.c` | — | Read test.txt content |
| List files | `sdcard.c` | — | List /music/ directory |
| No card | `sdcard.c` | — | Graceful "No SD Card" message |

### 1.8 Antenna & Wireless (Priority: LOW for Phase 1)

| Task | Description | Pass Criteria |
|------|-------------|---------------|
| WiFi scan | Scan nearby APs | At least 1 AP found |
| BLE advertise | Start BLE advertising | Phone can see "CopilotDemoPi" |

### Phase 1 Milestone
```
✅ All LEDs working
✅ LCD displaying boot logo
✅ Both sensors reading correctly
✅ Encoder and buttons responding
✅ Battery voltage reading accurate
✅ MicroSD mounting and reading files
✅ WiFi scan successful
```

---

## Phase 2: Audio (Day 4-7)

*Goal: Play music through the speaker.*

### 2.1 I2S Output

| Task | Driver | Interface | Pass Criteria |
|------|--------|-----------|---------------|
| I2S init | `max98357a.c` (i2s_std) | GPIO9(BCLK), GPIO10(LRCLK), GPIO11(DIN) | No I2S errors |
| Sine wave | `max98357a.c` | — | 440Hz tone from speaker |
| Volume control | `max98357a.c` | — | Software volume 0-100% |

### 2.2 Bluetooth A2DP Sink

| Task | Component | Pass Criteria |
|------|-----------|---------------|
| A2DP init | ESP-ADF a2dp_sink | Device appears as "CopilotDemoPi" on phone |
| Pair & play | ESP-ADF a2dp_sink | Music plays through speaker |
| AVRCP metadata | ESP-ADF avrcp | Track title + artist displayed on LCD |
| Auto-reconnect | ESP-ADF a2dp_sink | Reconnects to last device within 10s |
| Volume sync | ESP-ADF + encoder | Encoder changes volume, syncs with phone |

### 2.3 SD Card Playback

| Task | Component | Pass Criteria |
|------|-----------|---------------|
| MP3 decode | ESP-ADF mp3_decoder | Play /music/test.mp3 |
| WAV decode | ESP-ADF wav_decoder | Play /music/test.wav |
| Playlist | `bt_audio.c` | Sequential playback, next/prev with encoder |
| File display | `display.c` | Show filename on LCD |

### Phase 2 Milestone
```
✅ Speaker produces clear audio
✅ Bluetooth pairing and playback working
✅ Track info displayed on LCD
✅ Encoder controls volume
✅ SD card MP3/WAV playback working
```

---

## Phase 3: UI & Integration (Day 8-14)

*Goal: Polished product experience with LVGL interface.*

### 3.1 LVGL Setup

| Task | Description | Pass Criteria |
|------|-------------|---------------|
| LVGL init | Configure for ST7789, 240×240, PSRAM buffer | LVGL demo runs smoothly |
| Touch — N/A | No touch on this LCD | — |
| Flush callback | SPI DMA transfer | No tearing, smooth refresh |

### 3.2 UI Screens (from SDD)

| Screen | Content | Trigger |
|--------|---------|---------|
| `SCREEN_BOOT` | Logo + version + LED test | Power on (3s) |
| `SCREEN_BT_AUDIO` | Track title, artist, volume bar | BT mode |
| `SCREEN_SD_AUDIO` | Filename, track#/total, progress | SD mode |
| `SCREEN_SENSORS` | Temp, humidity, pressure, IAQ, IMU | Standby mode |
| `SCREEN_SLEEP` | "Sleeping..." → LCD off | Entering sleep |

### 3.3 Mode System

| Mode | Entry | Behavior |
|------|-------|----------|
| `MODE_BLUETOOTH` | Default / OpsKey cycle | A2DP sink, display track info |
| `MODE_SDCARD` | OpsKey cycle | Play from SD, display filename |
| `MODE_STANDBY` | OpsKey cycle | Sensors active, display readings |
| `MODE_SLEEP` | Long press OpsKey >3s | Deep sleep, GPIO5 wake |

### 3.4 Power Management

| Task | Description | Pass Criteria |
|------|-------------|---------------|
| Backlight timeout | LCD off after 30s idle | Any input wakes backlight |
| Deep sleep entry | Long press OpsKey | Current < 1.5mA (PPK II) |
| Deep sleep wake | Press OpsKey | Boot to last mode < 3s |
| gpio_reset_pin | Reset all non-wake GPIOs before sleep | Verify with PPK II |
| Low battery | Warning at 10%, auto-sleep at 5% | LCD shows warning |

### 3.5 WiFi Provisioning (Optional)

| Task | Description | Pass Criteria |
|------|-------------|---------------|
| BLE provisioning | ESP-IDF wifi_provisioning | Phone app can configure WiFi |
| NTP time sync | lwIP SNTP | RTC time accurate after sync |
| HTTP radio | ESP-ADF http_stream | Play internet radio station |

### Phase 3 Milestone
```
✅ LVGL UI smooth and responsive
✅ All 5 modes working with OpsKey switching
✅ Deep sleep < 1.5mA, wake < 3s
✅ Boot time < 3s to usable UI
✅ All features integrated and stable
```

---

## Task Priority Matrix

| | Critical | Important | Nice to Have |
|---|----------|-----------|-------------|
| **Phase 1** | USB flash, LCD, I2C sensors | Encoder, ADC, SD card | WiFi scan |
| **Phase 2** | I2S audio, BT A2DP | AVRCP metadata, volume | SD playback |
| **Phase 3** | LVGL screens, mode system | Deep sleep, power mgmt | WiFi provisioning, HTTP radio |

---

## Development Tools

| Tool | Purpose |
|------|---------|
| **ESP-IDF v5.x** | Build system, drivers, FreeRTOS |
| **VS Code + Copilot** | Code editing with AI assistance |
| **Codex (Agent)** | Generate drivers from AGENTS.md + SDD |
| **ESP-ADF** | Bluetooth audio, codecs, pipelines |
| **LVGL** | UI framework |
| **BSEC2** | BME690 IAQ processing |
| **PPK II** | Power profiling |
| **ESP-IDF Monitor** | Serial console debugging |

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| ESP32 doesn't boot | Blocks everything | Check power rails, crystal, USB wiring |
| LCD wrong color | Cosmetic | Try RGB565 byte-swapped (known from Chac 2.2) |
| I2C devices not found | Sensor features blocked | Check pull-ups, addresses, bus scan |
| Audio distortion | Poor user experience | Check MAX98357A VDD (use 5V not 3.3V) |
| BT audio choppy | Poor user experience | Dedicate one core to audio, other to UI |
| Deep sleep too high | Battery life | gpio_reset_pin all GPIOs, check LCD SLPIN |
| SD card not mounting | Storage features blocked | Check SPI pins, try different card |
| BSEC2 library issues | IAQ not available | Fall back to raw gas resistance value |

---

## Estimated Timeline

| Phase | Duration | Dependencies |
|-------|----------|-------------|
| Board arrival | ~7 days from JLCPCB order | — |
| **Phase 1: Bringup** | 3 days | Board + components |
| **Phase 2: Audio** | 4 days | Phase 1 complete |
| **Phase 3: UI** | 7 days | Phase 2 complete |
| **Total** | **~14 days** after board arrives |

---

## Repository Structure

```
ProjectCopilotDemoPi/
├── Hardware/
│   ├── CopilotDemoPi_schematic_FINAL.pdf
│   ├── CopilotDemoPi_PCB_top_FINAL.jpg
│   └── CopilotDemoPi_PCB_bottom_FINAL.jpg
├── Firmware/
│   ├── AGENTS.md                      ← AI coding constraints
│   └── demopi-firmware/
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       ├── main/
│       │   ├── CMakeLists.txt
│       │   ├── main.c                 ← app_main()
│       │   ├── config.h               ← All GPIO/I2C/SPI defines
│       │   ├── drivers/
│       │   │   ├── i2c_bus.c / .h
│       │   │   ├── bme690.c / .h
│       │   │   ├── bmi270.c / .h
│       │   │   ├── lcd.c / .h
│       │   │   ├── max98357a.c / .h
│       │   │   ├── ws2812b.c / .h
│       │   │   ├── encoder.c / .h
│       │   │   ├── sdcard.c / .h
│       │   │   ├── battery.c / .h
│       │   │   └── led_pwm.c / .h
│       │   ├── app/
│       │   │   ├── bt_audio.c / .h
│       │   │   ├── display.c / .h
│       │   │   ├── env_monitor.c / .h
│       │   │   ├── motion.c / .h
│       │   │   └── power.c / .h
│       │   └── assets/
│       │       └── logo_rgb565.h
│       └── components/
│           ├── bsec2/                  ← Bosch BSEC2 static library
│           └── lvgl/                   ← LVGL UI framework
├── Docs/
│   ├── CopilotDemoPi_SDD.md
│   ├── CopilotDemoPi_hardware_resource.md
│   ├── CopilotDemoPi_BOM_final.md
│   └── CopilotDemoPi_ERC_fix_log.md
└── README.md
```

---

*CopilotDemoPi Firmware Development Plan v1.0*
*by Cysoft & Chac 🐕 | April 2026*
