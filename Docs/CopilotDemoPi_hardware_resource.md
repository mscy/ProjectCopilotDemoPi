# CopilotDemoPi — Hardware Resource Guide

> ESP32-S3 Smart Speaker with Environmental Monitoring
> Board: 85 × 56mm (Raspberry Pi form factor), 4-layer PCB
> by Cysoft & Chac 🐕 | April 2026

---

## 1. IO Map

### ESP32-S3-PICO-1-N8R8 (U2) — Complete Pin Assignment

| Pin | GPIO | Function | Dir | Net Label | Module |
|-----|------|----------|-----|-----------|--------|
| 4 | EN | Reset | In | EN_Chip_PU | Reset circuit |
| 5 | **GPIO0** | Boot button | In | GPIO0-Boot | SW (Boot) |
| 6 | **GPIO1** | Battery ADC | In | Batt_Check | R31+R32 divider |
| 7 | **GPIO2** | 🔴 LED Red (PWM) | Out | GPIO2_Red | D6 |
| 8 | **GPIO3** | I2C SDA (Sensors) | Bidir | ADC_BME280_SDA_I2C | BME690 + BMI270 |
| 9 | **GPIO4** | I2C SCL (Sensors) | Bidir | ADC_BME280_SCL_I2C | BME690 + BMI270 |
| 10 | **GPIO5** | OpsKey button (wake) | In | GPIO_5_OpsKey | SW1 |
| 11 | **GPIO6** | LCD SPI SCLK | Out | LCD_SCLK-GPIO6 | ST7789 |
| 12 | **GPIO7** | LCD SPI MOSI | Out | LCD_MOSI-GPIO7 | ST7789 |
| 13 | **GPIO8** | WS2812B RGB LED | Out | GPIO8-RGB | D3 |
| 14 | **GPIO9** | I2S BCLK | Out | GPIO9_BCLK | MAX98357A |
| 15 | **GPIO10** | I2S LRCLK | Out | GPIO10_LRCLK | MAX98357A |
| 16 | **GPIO11** | I2S DIN | Out | GPIO11_DIN | MAX98357A |
| 17 | **GPIO12** | Rotary Encoder A | In | EncoderA | SW5 |
| 18 | **GPIO13** | Rotary Encoder B | In | EncoderB | SW5 |
| 19 | **GPIO14** | LCD DC (Data/Cmd) | Out | RS->GPIO14 | ST7789 |
| 23 | **GPIO17** | LCD Backlight (PWM) | Out | LCD_BL_GPIO17 | Q2 gate |
| 24 | **GPIO18** | IMU Interrupt | In | GPIO18_IMU_Int | BMI270 INT1 |
| 25 | **GPIO19** | ⛔ USB D- | Bidir | GPIO19_USB_D- | USB-C |
| 26 | **GPIO20** | ⛔ USB D+ | Bidir | GPIO20_USB_D+ | USB-C |
| 27 | **GPIO21** | LCD Reset | Out | Reset->GPIO21 | ST7789 |
| 38 | **GPIO33** | 🟢 LED Green (PWM) | Out | GPIO33_Green | D7 |
| 39 | **GPIO34** | 🔵 LED Blue (PWM) | Out | GPIO34_Blue | D8 |
| 40 | **GPIO35** | MicroSD MOSI | Out | GPIO35_SD_MOSI | J4 |
| 41 | **GPIO36** | MicroSD CLK | Out | GPIO36_SD_CLK | J4 |
| 42 | **GPIO37** | MicroSD CS | Out | GPIO37_SD_CS | J4 |
| 43 | **GPIO38** | MicroSD MISO | In | GPIO38_SD_MISO | J4 |
| 51 | **GPIO45** | 🔓 Spare (Strapping) | — | — | — |
| 52 | **GPIO46** | 🔓 Spare (Strapping) | — | — | — |

### Special Pins

| Pin | Function | Connection |
|-----|----------|------------|
| 1 | LNA_IN (Antenna) | IPEX connector (J2) |
| 21 | XTAL_32K_P | 32.768kHz Crystal (Y1) |
| 22 | XTAL_32K_N | 32.768kHz Crystal (Y1) |
| 2, 20, 46, 55 | VDD3P3 / VDD3P3_RTC / VDD3P3_CPU / VDDA | +3.3V |
| 3, GND pad | GND | Ground |
| 29 | VDD_SPI | Internal (Flash/PSRAM) |

### GPIO Summary

| Category | Count | GPIOs |
|----------|-------|-------|
| SPI LCD | 5 | 6, 7, 14, 17, 21 |
| I2S Audio | 3 | 9, 10, 11 |
| I2C Sensors | 3 | 3, 4, 18 |
| MicroSD SPI | 4 | 35, 36, 37, 38 |
| User Interface | 3 | 5, 12, 13 |
| USB | 2 | 19, 20 |
| LED | 4 | 2, 8, 33, 34 |
| ADC | 1 | 1 |
| Boot | 1 | 0 |
| **Spare** | **2** | **45, 46** |
| **Total** | **28** | |

---

## 2. I2C Bus

| Parameter | Value |
|-----------|-------|
| SDA | GPIO3 |
| SCL | GPIO4 |
| Pull-ups | 2× 4.7kΩ to 3.3V (R20, R22) |
| Speed | 400kHz (Fast mode) |

### I2C Devices

| Device | Address | Function | Ref |
|--------|---------|----------|-----|
| BME690 | **0x76** | Temp / Humidity / Pressure / IAQ | U1 |
| BMI270 | **0x68** | 6-axis IMU (Accel + Gyro) | U7 |

---

## 3. SPI LCD (ST7789)

| Parameter | Value |
|-----------|-------|
| Controller | ST7789V |
| Size | 1.54 inch |
| Resolution | 240 × 240 |
| Color | RGB565 (byte-swapped) |
| Interface | 4-wire SPI |
| Connector | J5, 8-pin FPC |

| Signal | GPIO | Pin |
|--------|------|-----|
| SCLK | GPIO6 | 11 |
| MOSI | GPIO7 | 12 |
| DC | GPIO14 | 19 |
| RST | GPIO21 | 27 |
| BL (PWM) | GPIO17 | 23 |

Backlight: GPIO17 → R15(100Ω) → Q2(AO3400A) gate → LCD BL

---

## 4. I2S Audio (MAX98357A)

| Parameter | Value |
|-----------|-------|
| Amplifier | MAX98357AETE+T (U6) |
| Type | I2S DAC + Class-D amp |
| Output | 1.8W @ 8Ω (5V supply) |
| Mode | L+R mono mix (SD_MODE: 1MΩ to VDD) |

| Signal | GPIO | Pin |
|--------|------|-----|
| BCLK | GPIO9 | 14 |
| LRCLK | GPIO10 | 15 |
| DIN | GPIO11 | 16 |

Speaker: J_SPK, JST 1.25mm 2-pin → 3020BOXCF-8Ω1W

---

## 5. MicroSD Card (SPI Mode)

| Signal | GPIO | Pin |
|--------|------|-----|
| MOSI (CMD) | GPIO35 | 40 |
| CLK | GPIO36 | 41 |
| CS (DAT3) | GPIO37 | 42 |
| MISO (DAT0) | GPIO38 | 43 |

Connector: J4, push-push MicroSD socket

---

## 6. Environmental Sensor — BME690 (U1)

| Parameter | Value |
|-----------|-------|
| Measurements | Temperature, Humidity, Pressure, IAQ (VOC) |
| Interface | I2C (0x76), shared bus with BMI270 |
| Accuracy | Temp ±0.5°C, Hum ±3% RH, Press ±0.5 hPa |
| IAQ | 0-500 index via Bosch BSEC library |
| Package | LGA-8 (3×3mm) |

---

## 7. IMU — BMI270 (U7)

| Parameter | Value |
|-----------|-------|
| Type | 6-axis (3-axis accel + 3-axis gyro) |
| Interface | I2C (0x68), shared bus with BME690 |
| Accel range | ±2/4/8/16g |
| Gyro range | ±125~2000°/s |
| INT1 | GPIO18 (data ready interrupt) |
| Package | LGA-14 (2.5×3mm) |

---

## 8. Power System

### Power Chain

```
USB-C 5V → BQ24072 (U3) → Battery / PM_VOut → TPS63001 (U4) → 3.3V
                │
           JST-PH 2P (J3)
           3.7V 1S Li-Ion
```

### Battery Charger — BQ24072RGT (U3)

| Parameter | Value |
|-----------|-------|
| Input | 5V USB |
| Charge current | ~500mA (ISET: R4=2kΩ) |
| Input limit | ~500mA (ILIM: R40=2kΩ) |
| Status LEDs | D1 (Red, ~CHG), D2 (Green, ~PGOOD) |

### Voltage Regulator — TPS63001 (U4)

| Parameter | Value |
|-----------|-------|
| Type | Buck-Boost |
| Input | PM_VOut (2.5-5.5V) |
| Output | 3.3V fixed |
| Inductor | L1 (2.2µH) |

### Battery Monitoring

| Parameter | Value |
|-----------|-------|
| GPIO | GPIO1 (ADC) |
| Divider | R31+R32 (100kΩ+100kΩ) |
| Formula | VBAT = ADC_reading × 2 |

### Power Rails

| Rail | Voltage | Source | Consumers |
|------|---------|--------|-----------|
| +5V | 5.0V | USB-C | BQ24072, MAX98357A |
| PM_VOut | 3.0-4.2V | BQ24072 | TPS63001 input |
| +3.3V | 3.3V | TPS63001 | ESP32, sensors, LCD, SD, LEDs |
| VBAT | 3.0-4.2V | Li-Ion battery | BQ24072 BAT |

---

## 9. USB-C (J1)

| Parameter | Value |
|-----------|-------|
| Connector | USB-C 16-pin |
| CC resistors | R2, R3 (5.1kΩ) — UFP device mode |
| ESD protection | U5 (TPD2EUSB30A) on D+/D- |
| TVS | D5 (PESD5V0S1BA) on VBUS |
| Shield filter | R1 (1MΩ) + C1 (10nF) to GND |
| Data | GPIO19 (D-), GPIO20 (D+) — native USB |

---

## 10. User Interface

### Buttons

| Ref | Function | GPIO | Notes |
|-----|----------|------|-------|
| SW_Boot | Boot mode | GPIO0 | Hold + Reset = download mode |
| SW_Reset | Reset | EN | Resets chip |
| SW1 | OpsKey | GPIO5 | Deep sleep wake source (RTC GPIO) |

### Rotary Encoder (SW5)

| Signal | GPIO |
|--------|------|
| A | GPIO12 |
| B | GPIO13 |
| Push button | (integrated in encoder) |

### Status LEDs

| Ref | Color | Source | Function |
|-----|-------|--------|----------|
| D1 | Red | BQ24072 ~CHG | Charging |
| D2 | Green | BQ24072 ~PGOOD | Power good |
| D3 | RGB | GPIO8 (WS2812B) | Status indicator |
| D6 | 🔴 Red | GPIO2 (PWM) | Demo LED |
| D7 | 🟢 Green | GPIO33 (PWM) | Demo LED |
| D8 | 🔵 Blue | GPIO34 (PWM) | Demo LED |

---

## 11. RTC — 32.768kHz Crystal (Y1)

| Parameter | Value |
|-----------|-------|
| Crystal | 32.768kHz |
| Pins | XTAL_32K_P (Pin 21), XTAL_32K_N (Pin 22) |
| Load caps | C9, C10 (22pF) |
| Use | Deep sleep accurate timing, RTC wakeup |

---

## 12. Antenna

| Parameter | Value |
|-----------|-------|
| Type | IPEX/U.FL connector (J2) |
| Pin | LNA_IN (Pin 1) |
| Frequency | 2.4GHz (WiFi + BLE) |
| Note | Keep-out zone under antenna area on PCB |

---

## 13. Connector Summary

| Ref | Type | Function | Pins |
|-----|------|----------|------|
| J1 | USB-C 16P | Power + Data + Programming | 16 |
| J2 | IPEX/U.FL | 2.4GHz Antenna | 2 |
| J3 | JST-PH 2P | Battery (1S Li-Ion) | 2 |
| J4 | MicroSD socket | TF card (SPI mode) | 8+2 |
| J5 | FPC 8P | ST7789 LCD | 8 |
| J_SPK | JST 1.25mm 2P | Speaker output | 2 |

---

## 14. BOM Summary

| Category | Components | Count |
|----------|-----------|-------|
| MCU | ESP32-S3-PICO-1-N8R8 | 1 |
| Power | BQ24072 + TPS63001 + D4 + L1 | 4 |
| USB | USB-C + TPD2EUSB30A + PESD5V0 | 3 |
| Audio | MAX98357A | 1 |
| Display | FPC connector + Q2(AO3400A) | 2 |
| Sensors | BME690 + BMI270 | 2 |
| Storage | MicroSD socket | 1 |
| UI | 2× buttons + 1× rotary encoder | 3 |
| LED | WS2812B + 3× color LED + 2× status LED | 6 |
| Crystal | 32.768kHz + 2× load caps | 3 |
| Antenna | IPEX connector | 1 |
| Passive | Resistors + Capacitors | ~50 |
| **Total** | | **~77** |

---

## 15. PCB Specifications

| Parameter | Value |
|-----------|-------|
| Size | 85 × 56 mm |
| Layers | 4 |
| Stackup | Top Signal → GND → 3.3V → Bottom Signal |
| Min trace | 0.15mm (signal), 0.3mm (power) |
| Min spacing | 0.15mm |
| Via | 0.3mm drill / 0.6mm pad |
| Corner radius | 3mm |
| Mounting | 4× M2.5 (Raspberry Pi compatible) |
| Components | **Dual-side SMT** (Top: large ICs + connectors, Bottom: passives + small ICs) |
| Speaker | External (JST 1.25mm connector) |
| Antenna | External (IPEX connector) |

---

## 16. Strapping Pins

| GPIO | Boot-time Function | Default | After Boot |
|------|-------------------|---------|------------|
| GPIO0 | Boot mode select | Internal pull-up (normal boot) | Boot button |
| GPIO3 | JTAG source | Internal pull-down | I2C SDA |
| GPIO45 | VDD_SPI voltage | Internal pull-down (3.3V) | Spare |
| GPIO46 | ROM log print | Internal pull-down (print) | Spare |

---

*CopilotDemoPi v1.0 | 77 components | AI-Powered Hardware Design Demo*
*by Cysoft & Chac 🐕 | April 2026*
