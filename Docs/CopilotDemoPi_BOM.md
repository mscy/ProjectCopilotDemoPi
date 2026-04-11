# CopilotDemoPi — Bill of Materials (BOM)

> 84 components | LCSC part numbers for JLCPCB SMT assembly

---

## Active Components (ICs)

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| U2 | ESP32-S3-PICO-1-N8R8 | MCU SiP (dual-core 240MHz, 8MB Flash, 8MB PSRAM, WiFi+BLE) | LGA-56 7×7mm | C2913197 | 1 |
| U3 | BQ24072RGT | 1S Li-Ion battery charger with power path | VQFN-16 3×3mm | C12250 | 1 |
| U4 | TPS63001 | Buck-Boost DC/DC converter, 3.3V output | SON-10 | C130281 | 1 |
| U5 | TPD2EUSB30A | USB ESD protection | SOT-23-3 (DRT-3) | C114050 | 1 |
| U6 | MAX98357A | I2S DAC + Class-D audio amplifier, 3.2W | TQFN-16 3×3mm | C2682619 | 1 |
| U1 | BME690 | Environmental sensor (temp/humidity/pressure/IAQ) | LGA-8 3×3mm | C5765022 | 1 |
| U7 | BMI270 | 6-axis IMU (accelerometer + gyroscope) | LGA-14 2.5×3mm | C2656858 | 1 |

## Discrete Semiconductors

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| Q2 | AO3400A | N-MOSFET (LCD backlight switch) | SOT-23 | C20917 | 1 |
| D3 | WS2812B | RGB LED (NeoPixel) | PLCC-4 5×5mm | C2761795 | 1 |
| D4 | 1N4148W | Switching diode | SOD-123 | C81598 | 1 |
| D5 | PESD5V0S1BA,115 | TVS ESD protection diode | SOD-323 | C88660 | 1 |
| D1 | LED Red | Status LED (charging) | 0603 | C2286 | 1 |
| D2 | LED Green | Status LED (power good) | 0603 | C72043 | 1 |
| D6 | LED Red | PWM demo LED | 0603 | C2286 | 1 |
| D7 | LED Green | PWM demo LED | 0603 | C72043 | 1 |
| D8 | LED Blue | PWM demo LED | 0603 | C72041 | 1 |

## Passive Components — Resistors

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| R8 | 0Ω | Antenna jumper | 0402 | C17168 | 1 |
| R5, R15, R23, R24, R25 | 100Ω | LED/signal resistors | 0402 | C25076 | 5 |
| R14, R18 | 1kΩ | BQ24072 LED resistors | 0402 | C11702 | 2 |
| R4, R40 | 2kΩ | BQ24072 ISET/ILIM | 0402 | C12118 | 2 |
| R20, R22 | 4.7kΩ | I2C pull-ups | 0402 | C25900 | 2 |
| R2, R3 | 5.1kΩ | USB-C CC pull-downs | 0402 | C25905 | 2 |
| R6, R9, R10, R11, R12, R13, R17, R19, R21 | 10kΩ | Pull-ups / general | 0402 | C25744 | 9 |
| R16 | 100kΩ | LCD backlight pull-down | 0402 | C25741 | 1 |
| R31, R32 | 100kΩ | Battery voltage divider | 0402 | C25741 | 2 |
| R1, R7 | 1MΩ | USB shield filter / MAX98357A SD_MODE | 0402 | C26083 | 2 |

## Passive Components — Capacitors

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| C9, C10 | 22pF | 32.768kHz crystal load caps | 0402 | C1555 | 2 |
| C1, C26, C27, C28 | 10nF | Filter / decoupling | 0402 | C15195 | 4 |
| C3, C4, C8, C11, C15, C16, C17, C19, C20, C21, C22, C23, C25 | 100nF | Decoupling | 0402 | C1525 | 13 |
| C6 | 1µF | LDO decoupling | 0402 | C52923 | 1 |
| C12 | 4.7µF | BQ24072 decoupling | 0402 | C23733 | 1 |
| C2, C5, C7, C18 | 10µF | Power decoupling | 0805 | C15850 | 4 |
| C13, C14 | NC | Reserved (not mounted) | 0402 | — | 0 |

## Inductors

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| L1 | 2.2µH | TPS63001 buck-boost inductor | 0402/4×4mm | C1015 | 1 |

## Crystal

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| Y1 | 32.768kHz | RTC crystal | 3215 SMD | C5213671 | 1 |

## Connectors

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| J1 | USB-C 16P | USB Type-C receptacle (power + data) | Top mount | C2765186 | 1 |
| J2 | IPEX/U.FL | 2.4GHz antenna connector | SMD | C88374 | 1 |
| J3 | JST-PH 2P | Battery connector (1S Li-Ion) | P2.0mm horizontal | C131337 | 1 |
| J4 | MicroSD Socket | Push-push MicroSD card slot | SMD | C164170 | 1 |
| J5 | FPC 8P 0.5mm | ST7789 LCD connector | FPC horizontal | C262267 | 1 |
| J6 | Pin Header 1×3 | GPIO45/46 test header | P2.54mm vertical | C49257 | 1 |
| J7 | HY2.0-4P | M5Stack-compatible I2C expansion | P2.0mm horizontal | C160326 | 1 |
| J8 | HY2.0-2P | Speaker connector (8Ω) | P2.0mm horizontal | C160324 | 1 |

## Switches

| Ref | Value | Description | Package | LCSC Part # | Qty |
|-----|-------|-------------|---------|-------------|-----|
| SW1 | Tactile Switch | Reset / Boot button | SMD 4×4mm | C528967 | 1 |
| SW2 | Tactile Switch | OpsKey / user button | SMD 4×4mm | C528967 | 1 |
| SW5 | Rotary Encoder | Volume / navigation (with push switch) | Through-hole vertical | C470271 | 1 |

---

## BOM Summary

| Category | Count |
|----------|-------|
| ICs (MCU, charger, regulator, amp, sensors, ESD) | 7 |
| Discrete (MOSFETs, diodes, LEDs) | 9 |
| Resistors | 26 |
| Capacitors | 25 |
| Inductors | 1 |
| Crystal | 1 |
| Connectors | 8 |
| Switches | 3 |
| NC (not mounted) | 2 |
| **Total (mounted)** | **82** |

---

## JLCPCB Assembly Notes

- **PCB Layers:** 4
- **Board Size:** 85 × 56 mm
- **Components:** Top side only
- **SMT parts:** All 0402/0603/SOT-23/QFN/LGA (JLCPCB compatible)
- **Through-hole:** SW5 (rotary encoder), J6 (pin header) — hand solder
- **X-ray required:** U2 (LGA-56), U3 (VQFN-16), U4 (SON-10), U6 (TQFN-16)
- **Extended parts:** U2 (ESP32-S3-PICO-1), U1 (BME690), U7 (BMI270), U6 (MAX98357A)

---

## External Components (not on PCB)

| Item | Spec | Source |
|------|------|--------|
| Speaker | 8Ω 1W, 30×20mm BOX, HY2.0-2P connector | 3020BOXCF-8Ω1W-2P1.25 |
| LCD Module | ST7789 1.54" 240×240, 8-pin FPC | Standard module |
| Battery | 3.7V 1S Li-Ion, 1000mAh, JST-PH 2P | Standard cell |
| Antenna | 2.4GHz IPEX/U.FL external antenna | Standard antenna |
| MicroSD Card | Up to 32GB | Standard card |

---

*CopilotDemoPi BOM v1.0 | by Cysoft & Chac 🐕 | April 2026*
