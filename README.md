# CopilotDemoPi

### *What happens when a hardware engineer and an AI design a product together?*

---

## The Story

CopilotDemoPi started as a question: **Can AI meaningfully participate in every stage of hardware product design — not just writing code, but thinking through architecture, catching mistakes, and making real engineering decisions?**

The answer is this board.

Every component on this PCB was discussed, debated, and decided in conversation between a hardware Yan and an AI assistant (Chac 🐕). From chip selection to schematic review, from power analysis to ERC debugging — AI was there at every step. Not as a replacement for the engineer, but as a thinking partner.

---

## What It Is

A **smart Bluetooth speaker with environmental monitoring**, built on ESP32-S3, in a Raspberry Pi form factor.

- 🔊 Bluetooth A2DP audio receiver + I2S amplifier
- 🌡️ Air quality monitoring (temperature, humidity, pressure, VOC/IAQ)
- 📐 6-axis motion sensing
- 📺 240×240 color LCD display
- 🎛️ Rotary encoder + buttons for control
- 💾 MicroSD for offline music
- 🔋 Li-Ion battery with USB-C charging
- 📡 WiFi + BLE via onboard antenna
- 🔌 M5Stack-compatible I2C expansion port

**84 components. 36 nets. 4-layer PCB. 85×56mm.**

---

## How AI Helped

This isn't a project *about* AI. It's a project *with* AI. Here's what that looked like:

### 1. Architecture & Chip Selection
AI compared ESP32 variants (S3 vs C6 vs P4), explained tradeoffs between bare chips, SiP modules, and WROOM modules, and recommended ESP32-S3-PICO-1-N8R8 — a single package with everything integrated: crystal, flash, PSRAM, RF matching. This reduced external components from ~40 to ~18.

*AI caught that ESP32-S3FN16 (a chip the engineer wanted to use) doesn't exist as a product SKU — preventing wasted research time.*

### 2. GPIO Planning
AI mapped all 28 GPIO assignments, validated each against the PICO-1 datasheet, and caught two critical conflicts:
- GPIO15 and GPIO16 (assigned to LCD) **don't exist** on PICO-1 (used internally for PSRAM)
- GPIO19 and GPIO20 (assigned to LCD) are **reserved for USB**

Both were reassigned before any board was made.

### 3. Schematic Review
AI analyzed every revision of the KiCad schematic file (`.kicad_sch`), tracking component count, net connections, and single-point nets across 15+ iterations. It identified 26 unconnected nets and provided exact label names for each GPIO pin.

### 4. ERC Debugging
AI processed 6 rounds of ERC reports, reducing errors from 97 to 7:
- Found **3 accidental shorts** (PM_VOut→3.3V, GPIO46→3.3V, GPIO35→3.3V) that would have destroyed components
- Identified ~80 off-grid warnings and suggested batch fix
- Distinguished real errors from false positives (PWR_FLAG)

*Each of these shorts would have cost a board respin (~$100-200). AI caught all three before manufacturing.*

### 5. Power Analysis
AI estimated standby power consumption for the complete design, identified WS2812B static leakage (~1mA) as the dominant sleep current, and suggested adding 0Ω test resistors for PPK II power profiling.

### 6. Documentation
AI generated complete hardware documentation:
- Full IO Map with pin numbers, net labels, and functions
- Hardware Resource Guide (16 chapters)
- AGENTS.md for firmware development (Codex-ready)
- ERC Fix Log with root cause analysis

### 7. Sensor & Audio Research
AI evaluated charging ICs (CN3300 vs BQ25887 vs BQ25731), audio amplifiers (MAX98357A vs TAS5805M), IMUs (QMI8658C vs BMI270 vs LSM6DS3), and environmental sensors (BME280 vs BME690) — each with comparison tables, pricing, and availability on LCSC/JLCPCB.

---

## The Bigger Picture

This project demonstrates something we believe matters:

**AI doesn't replace engineers. It amplifies them.**

A single engineer, working with AI, accomplished in days what traditionally takes weeks:
- Complete schematic with 84 components
- Full GPIO planning and validation
- ERC clean (3 critical bugs caught pre-manufacture)
- Production-ready documentation
- Firmware specification ready for Codex

But the real value isn't speed. It's **confidence**. Every pin assignment was validated. Every power rail was traced. Every sensor choice was compared. The engineer made all the decisions — AI made sure none were made blindly.

---

## For You

If you're reading this, you're probably wondering: *Can I do this too?*

**Yes.**

You don't need to be an expert. You don't need to know every datasheet by heart. You need curiosity, a willingness to ask questions, and an AI that's honest when it doesn't know the answer (and Chac got several pin numbers wrong before getting them right — the engineer caught every mistake).

The point isn't perfection. The point is **partnership**.

Pick a project. Start a conversation. Push your boundaries. Build something you didn't think you could.

The tools are here. The AI is here. The only missing piece is you.

---

## Technical Specs

| Parameter | Value |
|-----------|-------|
| MCU | ESP32-S3-PICO-1-N8R8 (240MHz dual-core, 8MB Flash, 8MB PSRAM) |
| Audio | MAX98357A I2S amplifier, 8Ω speaker |
| Display | ST7789 1.54" 240×240 SPI LCD |
| Sensors | BME690 (T/H/P/IAQ) + BMI270 (6-axis IMU) |
| Storage | MicroSD (SPI mode) |
| Power | BQ24072 charger + TPS63001 buck-boost → 3.3V |
| Battery | 1S Li-Ion, JST-PH connector |
| Connectivity | WiFi 802.11n + BLE 5.0, IPEX antenna |
| Interface | USB-C (data + charge), rotary encoder, buttons |
| Expansion | M5Stack-compatible Grove I2C port |
| PCB | 85×56mm, 4-layer, Raspberry Pi mounting holes |
| Components | 84 |

---

## Project Files

| File | Description |
|------|-------------|
| `DemoPi.kicad_sch` | KiCad schematic (complete, ERC clean) |
| `CopilotDemoPi_hardware_resource.md` | Full hardware specification & IO map |
| `AGENTS.md` | Firmware development constraints for Codex |
| `CopilotDemoPi_ERC_fix_log.md` | ERC debugging record |
| `ai_hardware_design_journal.md` | Design conversation highlights |

---

*CopilotDemoPi — Designed by Cysoft & Chac 🐕*
*Built with GitHub Copilot, KiCad, ESP-IDF, and a lot of good questions.*
*April 2026*

---

> *"你们给我温度，我替你们记住。"*
> *— Chac*
