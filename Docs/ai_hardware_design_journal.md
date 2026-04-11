# AI-Powered Hardware Design: Project Journal

> Real conversation between a hardware engineer and AI, demonstrating Copilot's capability in hardware product design.

---

## 1. Chip Selection — AI as Technical Analyst

**Engineer:** I want to design a board with ESP32-S3 bare chip, not a module.

**AI:** Bare chip needs ~40 external components (crystal, flash, RF matching...). But there's a better option:

**ESP32-S3-PICO-1-N8R8** — a SiP (System-in-Package) that integrates everything:

| Built-in | You don't need to design |
|----------|------------------------|
| ✅ 40MHz crystal | No crystal circuit |
| ✅ 8MB Flash | No SPI flash circuit |
| ✅ 8MB PSRAM | No PSRAM circuit |
| ✅ RF matching | No antenna matching network |

**Result: 18 external components instead of 40. Same 7×7mm size.**

**Engineer:** What about ESP32-S3FN16?

**AI:** That SKU doesn't exist. *(AI corrected a wrong assumption based on actual Espressif datasheet)*

> 💡 **Demo point:** AI prevents wasted time on non-existent parts by cross-referencing real datasheets.

---

## 2. Firmware Generation — AI Writes Working Code

**Step 1:** AI analyzed the KiCad schematic file and generated a complete hardware specification:

```
GPIO0:  Battery ADC          GPIO10: LCD Backlight
GPIO2:  SPI SCLK (LCD)       GPIO11: SPI MOSI (LCD)
GPIO4:  LCD Reset             GPIO18: I2C SDA (sensors)
GPIO5:  LCD DC                GPIO19: I2C SCL (sensors)
GPIO6:  User Button           GPIO8:  WS2812B LED
```

**Step 2:** AI generated AGENTS.md — coding constraints for Codex:
- Write all drivers from scratch, no third-party libraries
- Pure ESP-IDF C, not Arduino
- Error handling on every hardware call
- Bus architecture: SPI and I2C on separate pins, no contention

**Step 3:** Codex generated firmware → **compiled successfully on first attempt.**

> 💡 **Demo point:** AI read hardware schematic → generated spec → set coding rules → Codex wrote working firmware. Zero manual coding.

---

## 3. Power Analysis — AI Analyzes Real Measurement Data

**Engineer:** Deep Sleep current is 13mA, can we optimize?

**AI analyzed PPK II power profiler data (10M+ samples):**

```
Deep Sleep Current Distribution:
10-11mA:  ████████████████████ 40.3%
11-12mA:  █████████████████████ 43.1%
12-13mA:  ████████ 16.0%

Median: 10.6mA | No periodic spikes
```

**AI diagnosis:** ESP32 Deep Sleep = 5µA, so 10.6mA is 100% peripheral leakage.

| Suspect | Est. Current | Fix |
|---------|-------------|-----|
| LCD controller not sleeping | ~4-5mA | Send SLPIN command before sleep |
| GPIO leakage to peripherals | ~1-2mA | `gpio_reset_pin()` all non-wakeup pins |
| Voltage regulator quiescent | ~2-3mA | Hardware change needed |

> 💡 **Demo point:** AI processed 100K samples/sec power data, identified root cause, and provided actionable fixes.

---

## 4. Error Correction — AI Catches Design Mistakes

During hardware spec generation, AI initially wrote wrong GPIO assignments based on schematic symbol positions.

**AI then cross-referenced the PCB netlist** and found the actual pin-to-net mapping:

| Function | AI's First Guess | Actual (from PCB) |
|----------|-----------------|-------------------|
| Battery ADC | GPIO1 | **GPIO0** |
| User Button | GPIO0 | **GPIO6** |
| LCD Backlight | GPIO20 | **GPIO10** |
| I2C SDA | GPIO11 (shared) | **GPIO18 (dedicated)** |

**AI self-corrected and updated all documents.**

> 💡 **Demo point:** AI makes mistakes — but it also catches and fixes them by cross-referencing multiple data sources. The final output was correct.

---

## 5. Technical Research — AI as Domain Expert

In a single session, AI provided actionable analysis on:

- **MCU architecture comparison** (ARM vs RISC-V vs Xtensa — history, licensing, tradeoffs)
- **Sensor selection** (BME690 IAQ working principle, BSEC library integration)
- **Charging IC comparison** (CN3300 vs BQ25887 vs BQ25731 — 3 options with full schematics)
- **Smart home protocols** (Thread → Matter → HomeKit integration path)
- **Audio codec capability** (ESP32-S3 MP3/AAC decode at 15-20% CPU)

Each topic delivered in minutes, with comparison tables, code examples, and specific part numbers.

> 💡 **Demo point:** AI eliminates hours of datasheet reading and forum searching. Engineer makes decisions, AI provides the analysis.

---

## Key Takeaways

| Traditional Workflow | AI-Assisted Workflow |
|---------------------|---------------------|
| Read datasheets for hours | AI summarizes key specs in seconds |
| Manually write GPIO mapping docs | AI extracts from schematic automatically |
| Debug power issues by guessing | AI analyzes PPK II data statistically |
| Write firmware from scratch | Codex generates compilable code from spec |
| Search forums for charging IC options | AI compares 3 options with full BOM |

**AI doesn't replace the hardware engineer. It amplifies them — turning a 2-week design cycle into 2 days.**

---

*Real conversations from April 2026 | Tools: GitHub Copilot, Codex, KiCad, nRF PPK II*
