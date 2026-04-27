# Copilot Demo Pi v1.0 — Hardware Specification

> **Version**: 1.0  
> **Date**: 2026-04-27  
> **Authors**: Yan Cui (CuiYan) & Chac 🐕 (@ChihuahuaChac)  
> **Repository**: https://github.com/mscy/ProjectCopilotDemoPi/  
> **KiCad**: 9.0

---

## 1. Overview

A smart Bluetooth speaker with environmental monitoring, built on ESP32-S3, in a Raspberry Pi form factor. Every component was discussed, debated, and decided in conversation between a hardware engineer and an AI assistant.

### Key Features
- 🔊 Bluetooth A2DP audio receiver + I2S Class-D amplifier
- 🌡️ Air quality monitoring (temperature, humidity, pressure, VOC/IAQ)
- 📐 6-axis motion sensing (accelerometer + gyroscope)
- 📺 240×240 color LCD display (ST7789)
- 🎛️ Rotary encoder + buttons for control
- 💾 MicroSD card for offline music/data logging
- 🔋 Li-Ion battery with USB-C charging + buck-boost regulation
- 📡 WiFi + BLE via onboard antenna
- 🔌 M5Stack-compatible I2C expansion port
- 🌈 WS2812B RGB LED + individual R/G/B status LEDs

---

## 2. Board Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 85 × 56 mm (Raspberry Pi form factor) |
| Layers | 4 |
| Total components | 88 |
| Unique nets | 36 |
| Mounting holes | 4 (M2.5, Pi-compatible) |
| Power input | USB-C 5V |
| Battery | 3.7V Li-Ion (JST 2P connector) |
| Regulated output | 3.3V (buck-boost) |

---

## 3. Major ICs

| Ref | IC | Function | Package |
|-----|-----|---------|---------|
| U2 | **ESP32-S3-PICO-1-N8R8** | MCU — WiFi/BLE, 8MB Flash, 8MB PSRAM, integrated crystal & RF matching | LGA |
| U1 | **BME690** | Environmental sensor — temp, humidity, pressure, VOC/IAQ | LGA-8 |
| U7 | **BMI270** | 6-axis IMU — accelerometer + gyroscope | LGA-14 |
| U6 | **MAX98357A** | I2S Class-D mono amplifier (3.2W) | QFN-16 |
| U3 | **BQ24072RGT** | Li-Ion battery charger (USB-C input) | QFN-16 |
| U4 | **TPS63001** | Buck-boost regulator (3.3V output from 1.8~5.5V) | QFN-10 |
| U5 | **TPD2EUSB30A** | USB ESD protection | SOT-6 |

---

## 4. ESP32-S3 GPIO Assignment

### Audio (I2S → MAX98357A)
| GPIO | Function | Connected To |
|------|----------|-------------|
| GPIO9 | I2S_BCLK | MAX98357A BCLK |
| GPIO10 | I2S_LRCLK | MAX98357A LRCLK |
| GPIO11 | I2S_DIN | MAX98357A DIN |

### Display (SPI → ST7789 LCD)
| GPIO | Function | Connected To |
|------|----------|-------------|
| GPIO6 | LCD_SCLK | ST7789 SCK |
| GPIO7 | LCD_MOSI | ST7789 SDA |
| GPIO14 | LCD_DC (RS) | ST7789 RS |
| GPIO17 | LCD_BL | Backlight control |
| GPIO21 | LCD_RESET | ST7789 Reset |

### Sensors (I2C — shared bus)
| GPIO | Function | Connected To |
|------|----------|-------------|
| I2C_SCL | ADC_BME280_SCL_I2C | BME690 + BMI270 + M5Stack ExtI2C |
| I2C_SDA | ADC_BME280_SDA_I2C | BME690 + BMI270 + M5Stack ExtI2C |
| GPIO18 | IMU_INT | BMI270 interrupt |

### MicroSD Card (SPI)
| GPIO | Function | Connected To |
|------|----------|-------------|
| GPIO35 | SD_MOSI | MicroSD DI |
| GPIO36 | SD_CLK | MicroSD CLK |
| GPIO37 | SD_CS | MicroSD CS |
| GPIO38 | SD_MISO | MicroSD DO |

### User Interface
| GPIO | Function | Connected To |
|------|----------|-------------|
| Encoder A | EncoderA | EC11 rotary encoder |
| Encoder B | EncoderB | EC11 rotary encoder |
| GPIO5 | OpsKey | Encoder push button |
| GPIO0 | Boot | Boot button (SW1) |

### LEDs
| GPIO | Function | Connected To |
|------|----------|-------------|
| GPIO8 | WS2812B RGB | Addressable RGB LED (D3) |
| GPIO2 | Red LED | D6 status LED |
| GPIO33 | Green LED | D7 status LED |
| GPIO34 | Blue LED | D8 status LED |

### USB
| GPIO | Function | Connected To |
|------|----------|-------------|
| USB_D+ | USB Data+ | USB-C connector (via TPD2EUSB30A) |
| USB_D- | USB Data- | USB-C connector (via TPD2EUSB30A) |

### Expansion
| GPIO | Function | Connected To |
|------|----------|-------------|
| GPIO45 | Ext GPIO | J6 ExtGPIO header |
| GPIO46 | Ext GPIO | J6 ExtGPIO header |
| I2C_SCL/SDA | M5Stack I2C | J7 M5Stack ExtI2C (Grove compatible) |

### Battery Monitoring
| GPIO | Function | Connected To |
|------|----------|-------------|
| Batt_Check | ADC | Battery voltage divider |

---

## 5. Power Architecture

```
USB-C 5V ──→ BQ24072 (U3) ──→ Li-Ion Battery (3.7V)
                │                    │
                └──── OR ────────────┘
                         │
                    TPS63001 (U4) Buck-Boost
                         │
                      3.3V (PM_VOut)
                         │
                    ┌─────┼─────┬─────┬─────┐
                   ESP32  BME  BMI   LCD   MAX98357
```

### BQ24072RGT — Battery Charger
| Parameter | Value |
|-----------|-------|
| Input | USB-C 5V |
| Chemistry | Li-Ion/Li-Po single cell |
| Charge current | Programmable (R4 2kΩ → ~500mA) |
| Features | Power path management, thermal regulation |

### TPS63001 — 3.3V Buck-Boost Regulator
| Parameter | Value |
|-----------|-------|
| Input | 1.8V ~ 5.5V (battery or USB) |
| Output | 3.3V fixed |
| Max current | 1.2A |
| Inductor | L1 2.2µH |
| Efficiency | >90% typical |

### Battery Monitoring
- Voltage divider (R31 100kΩ / R32 100kΩ) → ESP32 ADC
- Batt_Check net reads 50% of battery voltage

---

## 6. Audio Subsystem

### MAX98357A (U6) — I2S Class-D Amplifier
| Parameter | Value |
|-----------|-------|
| Interface | I2S (BCLK, LRCLK, DIN) |
| Output power | 3.2W @ 4Ω, 1.8W @ 8Ω |
| Supply | 3.3V |
| Speaker | J8, 2-pin connector (SPK+/SPK-) |
| Gain | Configurable via GAIN pin |

Audio path:
```
ESP32 BT A2DP → I2S → MAX98357A → Speaker
                                → or SD card playback
```

---

## 7. Sensors

### BME690 (U1) — Environmental Sensor
| Parameter | Value |
|-----------|-------|
| Interface | I2C (shared bus) |
| Temperature | -40 ~ +85°C, ±0.5°C |
| Humidity | 0 ~ 100%, ±3% |
| Pressure | 300 ~ 1100 hPa, ±0.6 hPa |
| Gas (VOC) | IAQ index |
| Supply | 3.3V |

### BMI270 (U7) — 6-Axis IMU
| Parameter | Value |
|-----------|-------|
| Interface | I2C (shared bus) |
| Accelerometer | ±2/4/8/16g |
| Gyroscope | ±125/250/500/1000/2000 dps |
| Interrupt | GPIO18 (IMU_INT) |
| Supply | 3.3V |

---

## 8. Display & UI

### ST7789 1.54" LCD (J5)
| Parameter | Value |
|-----------|-------|
| Resolution | 240 × 240 |
| Interface | SPI (SCLK, MOSI, DC, Reset) |
| Backlight | PWM controlled (GPIO17) |
| Connector | 8-pin header |

### EC11 Rotary Encoder (SW5)
| Signal | GPIO |
|--------|------|
| Encoder A | EncoderA |
| Encoder B | EncoderB |
| Push button | GPIO5 (OpsKey) |

### Buttons
| Button | GPIO | Function |
|--------|------|----------|
| SW1 | GPIO0 | Boot / user button |
| SW2 | EN_Chip_PU | Reset / Chip enable |

### LEDs
| LED | Type | GPIO |
|-----|------|------|
| D3 | WS2812B RGB | GPIO8 |
| D6 | Red | GPIO2 |
| D7 | Green | GPIO33 |
| D8 | Blue | GPIO34 |
| D1 | Red (power/charge) | BQ24072 status |
| D2 | Green (power/charge) | BQ24072 status |

---

## 9. Connectors

| Ref | Connector | Function |
|-----|-----------|----------|
| J1 | USB-C 16P | Power input + USB data |
| J2 | Coaxial (C88374) | External antenna |
| J3 | JST 2P | 3.7V Li-Ion battery |
| J4 | MicroSD slot (C164170) | Storage |
| J5 | 8-pin header | ST7789 LCD |
| J6 | 4-pin header | ExtGPIO (GPIO45/46 + 3.3V + GND) |
| J7 | 4-pin header | M5Stack ExtI2C (I2C + 3.3V + GND) |
| J8 | JST 2P | Speaker output |

---

## 10. Protection

| Device | Ref | Function |
|--------|-----|----------|
| TPD2EUSB30A | U5 | USB D+/D- ESD protection |
| PESD5V0S1BA | D5 | USB VBUS TVS protection |
| 1N4148W | D4 | Reverse polarity / OR-ing diode |

---

## 11. Passive Components Summary

| Type | Count | Values |
|------|-------|--------|
| Capacitors | 27 | 22pF, 10nF, 100nF, 0.1µF, 1µF, 4.7µF, 10µF, NC×2 |
| Resistors | 28 | 0Ω, 100Ω, 1kΩ, 2kΩ, 4.7kΩ, 5.1kΩ, 10kΩ, 100kΩ, 1MΩ |
| Inductors | 1 | 2.2µH (TPS63001) |
| Crystal | 1 | C5213671 (32.768kHz RTC) |

---

## 12. BOM Summary

| Category | Components | Count |
|----------|-----------|-------|
| MCU | ESP32-S3-PICO-1-N8R8 | 1 |
| Sensors | BME690, BMI270 | 2 |
| Audio | MAX98357A | 1 |
| Power | BQ24072, TPS63001 | 2 |
| Protection | TPD2EUSB30A, PESD5V0S1BA, 1N4148W | 3 |
| Connectors | USB-C, antenna, battery, SD, LCD, GPIO, I2C, speaker | 8 |
| LEDs | WS2812B, Red×2, Green×2, Blue×1 | 6 (inc. D1/D2 charge status) |
| Switches | Boot, Reset, EC11 encoder | 3 |
| Passive | Capacitors, resistors, inductor, crystal | 57 |
| Mechanical | Mounting holes | 4 |
| **Total** | | **88** |

---

## 13. Design Notes

1. **ESP32-S3-PICO-1-N8R8** — SiP module with integrated crystal, flash (8MB), PSRAM (8MB), and RF matching. Reduced external component count from ~40 to ~18.

2. **C13/C14 = NC** — Crystal capacitor positions reserved but not populated (PICO-1 has internal crystal).

3. **R8 = 0Ω** — Antenna selection jumper. Solder to use PCB antenna, leave open for external antenna via J2.

4. **I2C bus shared** between BME690, BMI270, and M5Stack expansion port (J7). Pull-ups R20/R22 (4.7kΩ).

5. **Battery voltage monitoring** via R31/R32 (100kΩ/100kΩ divider) → ESP32 ADC.

6. **BQ24072 power path** — Seamless switching between USB and battery power.

7. **4 mounting holes** — M2.5, Raspberry Pi compatible spacing.

---

> *"This isn't a project about AI. It's a project with AI."*
