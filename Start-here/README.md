# Start Here

![CopilotDemoPi Concept Sketch](../Hardware/CopilotDemoPi_concept_sketch.png)

Welcome to **CopilotDemoPi** — a smart Bluetooth speaker with environmental monitoring, built on ESP32-S3 in a Raspberry Pi form factor. This guide will help you get oriented and start contributing.

---

## Repo Layout

```
CopilotDemoPi/
├── democode/           ESP-IDF firmware source code (build this!)
├── Firmware/           Firmware specs, flashing guide, and pre-built binary
│   ├── AGENTS.md       Coding rules for firmware development (read this first!)
│   ├── FLASHING.md     How to flash the pre-built binary
│   └── *.bin           Pre-compiled firmware image
├── Docs/               Design documents and references
│   ├── CopilotDemoPi_hardware_resource.md   Full hardware spec & IO map
│   ├── CopilotDemoPi_BOM.md                 Bill of materials
│   ├── CopilotDemoPi_SDD.md                 Software design document
│   ├── CopilotDemoPi_firmware_plan.md       Firmware development plan
│   ├── CopilotDemoPi_ERC_fix_log.md         ERC debugging record
│   └── ai_hardware_design_journal.md        AI-assisted design story
├── Hardware/           PCB photos, schematics (PDF), and logo assets
├── Start-here/         You are here!
└── README.md           Project overview and technical specs
```

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) | v5.x | Build & flash firmware |
| [KiCad](https://www.kicad.org/) | 8.x+ | View/edit schematics and PCB layout |
| [Git](https://git-scm.com/) | 2.x+ | Version control |
| USB-C cable | — | Flash and debug via USB |

> **Note:** If you only want to review the hardware design, KiCad and a PDF viewer are sufficient — no ESP-IDF needed.

---

## Build, Flash & Run with VS Code

This section walks you through the complete workflow — from cloning the repo to seeing output on the board — using **Visual Studio Code** with the **ESP-IDF extension**.

> **First time?** Follow the [Environment Setup Guide](ENVIRONMENT_SETUP.md) to install VS Code and the ESP-IDF toolchain before continuing.

### Step 1 — Clone the repo

```bash
git clone https://github.com/mscy/ProjectCopilotDemoPi.git
```

### Step 2 — Open the firmware project in VS Code

Open the `democode/` folder (not the repo root) as your workspace:

```bash
cd ProjectCopilotDemoPi
code democode
```

Or in VS Code: **File → Open Folder…** → select the `democode/` directory.

> **Why `democode/`?** This folder contains the ESP-IDF project root (`CMakeLists.txt`, `main/`, `sdkconfig`, etc.). The ESP-IDF extension needs to be at the project root to work correctly.

### Step 3 — Select the target chip

1. Open the Command Palette: `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
2. Run: **ESP-IDF: Set Espressif Device Target**
3. Select **esp32s3**

### Step 4 — Build the firmware

**Option A — Click the build icon:**

Look at the bottom status bar in VS Code. Click the **🔨 Build** button (provided by the ESP-IDF extension).

**Option B — Use the Command Palette:**

1. Open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`)
2. Run: **ESP-IDF: Build your Project**

**Option C — Use the integrated terminal:**

```bash
idf.py build
```

> The first build may take several minutes. Subsequent builds are incremental and much faster.

### Step 5 — Connect the board

1. Connect the CopilotDemoPi board to your computer via a **USB-C data cable**
2. The ESP-IDF extension should auto-detect the serial port
3. If not, click the **port selector** in the bottom status bar and choose the correct port:
   - macOS: `/dev/cu.usbmodem*`
   - Linux: `/dev/ttyACM0` or `/dev/ttyUSB0`
   - Windows: `COM3`, `COM4`, etc.

### Step 6 — Flash the firmware

**Option A — Click the flash icon:**

Click the **⚡ Flash** button in the bottom status bar.

**Option B — Use the Command Palette:**

Run: **ESP-IDF: Flash your Project**

**Option C — Terminal:**

```bash
idf.py -p /dev/cu.usbmodem101 flash
```

> If flashing fails with "No serial data received", enter download mode manually: hold **BOOT**, press **RESET**, release **BOOT**.

### Step 7 — Monitor serial output

**Option A — Click the monitor icon:**

Click the **📺 Monitor** button in the bottom status bar.

**Option B — One-click build + flash + monitor:**

Click the **🔥** (flame) button to build, flash, and open the serial monitor in one step.

**Option C — Terminal:**

```bash
idf.py -p /dev/cu.usbmodem101 monitor
```

Press `Ctrl+]` to exit the monitor.

### Expected Output

After flashing, the board reboots and you should see:

```
I (xxx) demopi: CopilotDemoPi — fw vX.X
I (xxx) demopi: ESP32-S3, 2 cores, rev X
I (xxx) lcd: ST7789 initialized (240x240)
I (xxx) bme690: sensor ready
...
```

The LCD will display the boot logo, LEDs will cycle through R/G/B, and the board enters the main UI.

### Quick Reference — VS Code Status Bar

| Button | Action | Shortcut |
|--------|--------|----------|
| 🔨 | Build | `Ctrl+E B` |
| ⚡ | Flash | `Ctrl+E F` |
| 📺 | Monitor | `Ctrl+E M` |
| 🔥 | Build + Flash + Monitor | `Ctrl+E D` |
| 🔌 | Select serial port | — |
| 🎯 | Select target (esp32s3) | — |

> For flashing a pre-built binary without building from source, see [`Firmware/FLASHING.md`](../Firmware/FLASHING.md).

---

## Key Documents

Start with these in order:

1. **[README.md](../README.md)** — Project story, specs, and how AI helped design the hardware
2. **[AGENTS.md](../Firmware/AGENTS.md)** — Firmware coding rules (language, style, drivers, error handling). **Read before writing any code.**
3. **[Hardware Resource Guide](../Docs/CopilotDemoPi_hardware_resource.md)** — Complete IO map, pin assignments, and peripheral details
4. **[Firmware Plan](../Docs/CopilotDemoPi_firmware_plan.md)** — Development roadmap and module breakdown
5. **[BOM](../Docs/CopilotDemoPi_BOM.md)** — Full bill of materials with part numbers

---

## Contribution Guidelines

### Code Style

All firmware code must follow the rules in [`AGENTS.md`](../Firmware/AGENTS.md). Key points:

- **Language:** C (not C++), using ESP-IDF v5.x
- **Naming:** `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for constants
- **Logging:** Use `ESP_LOGI()` / `ESP_LOGW()` / `ESP_LOGE()` — never `printf()`
- **Errors:** Every hardware function returns `esp_err_t`; check every return value
- **Drivers:** Write from scratch using ESP-IDF APIs — no third-party libraries (except LVGL, BSEC2, ESP-ADF)

### Branching & PRs

1. Create a feature branch from `main`
2. Make focused, well-tested commits
3. Open a pull request with a clear description of changes
4. Ensure the build passes (`idf.py build`) before submitting

### Hardware Changes

- Schematic and PCB edits should be made in KiCad
- Export updated PDFs for review
- Document any pin reassignments in the hardware resource guide

---

## FAQ

**Q: Do I need the physical board to contribute?**
A: Yes. This project is a showcase of how to design hardware products together with AI — from chip selection and schematic review to PCB layout and ERC debugging. Having the physical board lets you validate the full design cycle end-to-end.

**Q: Can I use Arduino or PlatformIO?**
A: No. This project uses ESP-IDF exclusively. See [AGENTS.md](../Firmware/AGENTS.md) for details.

**Q: Where are the KiCad project files?**
A: The schematic PDFs are in `Hardware/`. The full KiCad project files (`.kicad_sch`, `.kicad_pcb`) are referenced in the docs but may be in a separate location — check with the maintainers.

**Q: How do I add a new driver?**
A: Follow the structure in `AGENTS.md` § 5 (Project Structure). Place your driver in `main/drivers/`, name it `module.c` / `module.h`, prefix all functions with the module name, and add a self-test function.

**Q: Where can I find the pin assignments?**
A: See the `config.h` section in [AGENTS.md](../Firmware/AGENTS.md) or the full [Hardware Resource Guide](../Docs/CopilotDemoPi_hardware_resource.md).

---

*Ready to build? Start by reading [AGENTS.md](../Firmware/AGENTS.md), then pick a module from the [firmware plan](../Docs/CopilotDemoPi_firmware_plan.md). Happy hacking!*
