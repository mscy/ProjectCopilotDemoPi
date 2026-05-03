# CopilotDemoPi — Firmware Feature Summary

A self-contained sample firmware for the CopilotDemoPi board
(ESP32-S3-PICO-1-N8R8, 8 MB flash + 8 MB octal PSRAM). The image showcases
every on-board peripheral through a tabbed UI on the 240×240 ST7789 LCD,
driven by a rotary encoder and the OpsKey button.

---

## 1. Boot Sequence

1. NVS init, chip-info log, PSRAM free-heap report.
2. RGB LED self-test (red → green → blue) on the PWM demo LEDs.
3. Drivers initialized: I²C bus, LCD + backlight, BME690, BMI270, MAX98357A
   I²S, WS2812B, rotary encoder (PCNT), buttons, SD card (SPI), battery ADC.
4. Boot logo + version string drawn on the LCD.
5. Startup chime played through the speaker.
6. FreeRTOS tasks launched: sensor_task, control_task, power_task,
   tick_task, mp3_player_task.

---

## 2. UI — Five Sensor Tabs

The OpsKey (GPIO5) short-press cycles through the tabs; long-press enters
deep sleep. Each tab plays a click sound on entry.

### Tab 1 — BME690 Environment
- Live temperature (°C), humidity (%RH), pressure (hPa), gas resistance (Ω).
- IAQ index calculated by Bosch BSEC2 (closed-source library, linked as
  static archive). Falls back to a heuristic when BSEC2 is still warming up.
- Custom heater calibration: reads par_gh1/2/3, res_heat_range,
  res_heat_val from the BME690 trimming registers and computes
  RES_HEAT_0 per the BME68x datasheet (target 300 °C, ~150 ms gas wait).

### Tab 2 — BMI270 IMU Raw
- 3-axis accelerometer (m/s²) and gyroscope (°/s).
- Driver written from scratch; configures ±4 g / ±1000 °/s.

### Tab 3 — Balance / Tilt
- Bubble-level visualization driven by accelerometer X/Y.
- Useful as an interactive demo of IMU output.

### Tab 4 — Player
- MP3 playback from the MicroSD card (/sdcard, FATFS via VFS).
- 16-band FFT spectrum bars rendered live on the LCD.
- WS2812B reacts to bass/mid/treble band energy with HSV-cycling colors.
- Volume controlled by rotary encoder; tick sound on each detent.
- Powered by Espressif esp_audio_codec (locally vendored, no network
  dependency) feeding the MAX98357A I²S Class-D amplifier.

### Tab 5 — I²C Scanner
- Probes the full 7-bit address range (0x01–0x7F) on the sensor bus.
- LCD shows real-time progress bar and a grid of discovered addresses.
- Scan aborts immediately if the user switches tabs mid-scan; revisiting
  the tab triggers a re-scan.

---

## 3. Drivers (all hand-written, ESP-IDF only)

| Module | Peripheral | Notes |
|--------|------------|-------|
| i2c_bus | i2c_master (new driver) | 400 kHz, shared by BME690 + BMI270 |
| bme690 | I²C | Forced mode + dynamic heater calibration |
| bmi270 | I²C | ±4 g / ±1000 °/s; INT1 on GPIO18 |
| lcd | spi_master | ST7789, 240×240, RGB565 byte-swapped |
| max98357a | i2s_std | 44.1 kHz / 16-bit stereo (chip mixes to mono) |
| ws2812b | RMT | Hardware-timed; supports N-LED chains |
| encoder | PCNT | Hardware quadrature decoding, no debounce SW |
| buttons | GPIO + ISR | Boot button, OpsKey short/long detection |
| sdcard | spi_master + VFS/FATFS | Mounted at /sdcard |
| battery | esp_adc oneshot | GPIO1, ×2 divider, 10-sample average |
| led_pwm | LEDC | 3 channels for RGB demo LEDs + 1 for backlight |

---

## 4. Application Layer

- `mode` — global app mode (NORMAL / SLEEP).
- `power` — battery thresholds, low-voltage warning, deep sleep entry,
  OpsKey RTC wake source, GPIO reset before sleep.
- `display` — LVGL-free hand-rolled rendering with a built-in 5×7 font;
  one drawing function per screen (BME690, BMI270, balance, player,
  I²C scan, boot logo).
- `mp3_player` — decoder, ring buffer (PSRAM), 16-band FFT, band-energy
  meter, track selection, mute on tab exit.

---

## 5. Audio Pipeline

```
SD card (MP3) → esp_audio_codec → PCM ring buffer (PSRAM)
              → FFT (16 bands) → I²S → MAX98357A → speaker
```

- No Bluetooth stack — the firmware is fully offline.
- A short startup chime and a per-tap "click" provide UI feedback.

---

## 6. Power Management

- Long OpsKey press → graceful shutdown → esp_deep_sleep_start().
- OpsKey configured as RTC wake source.
- Battery monitored once per second; warning UI under BATT_VOLTAGE_LOW
  (3.50 V); auto-shutdown under BATT_VOLTAGE_SHUT (3.30 V).
- LCD backlight is PWM-driven and dims/turns off after idle timeout.

---

## 7. Memory Footprint

- App binary: ~979 KB (ProjectDemoPi_init.bin).
- Merged image (bootloader + partitions + app): ~1.1 MB
  (ProjectDemoPi_init-merged.bin).
- Partition: 2 MB app slot, 53 % free at flash time.
- Frame buffer + audio buffers placed in 8 MB octal PSRAM.

---

## 8. Build & Flash

- Toolchain: ESP-IDF v5.5.2, target esp32s3.
- Build: `idf.py build` from project root.
- Flash a single merged binary to offset 0x0 — see
  [FLASHING.md](FLASHING.md).

---

## 9. What This ROM Does NOT Do

- No Wi-Fi, no Bluetooth, no BLE.
- No cloud connectivity / OTA update.
- No third-party Arduino or PlatformIO libraries — every driver is written
  against ESP-IDF APIs only (exceptions: BSEC2 binary, esp_audio_codec).
