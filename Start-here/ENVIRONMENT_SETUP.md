# Environment Setup Guide

This guide walks you through setting up a complete development environment for CopilotDemoPi firmware on **macOS**, **Linux**, and **Windows**.

---

## Overview

You will install:

1. **Visual Studio Code** — Code editor with ESP-IDF integration
2. **ESP-IDF v5.x** — Espressif's official SDK and toolchain for ESP32-S3
3. **ESP-IDF VS Code Extension** — One-click build, flash, and monitor from the editor

> **Estimated time:** 30–60 minutes (depending on download speed).

---

## 1. Install Visual Studio Code

### macOS

1. Download VS Code from [https://code.visualstudio.com](https://code.visualstudio.com)
2. Open the `.dmg` file and drag **Visual Studio Code** into the **Applications** folder
3. Launch VS Code, then open the Command Palette (`Cmd + Shift + P`) and run:
   ```
   Shell Command: Install 'code' command in PATH
   ```

### Linux (Ubuntu / Debian)

```bash
# Download and install the .deb package
sudo apt update
sudo apt install -y wget
wget -qO code.deb 'https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64'
sudo dpkg -i code.deb
sudo apt install -f -y
rm code.deb
```

### Windows

1. Download the installer from [https://code.visualstudio.com](https://code.visualstudio.com)
2. Run the installer — check **"Add to PATH"** when prompted
3. Restart any open terminals after installation

### Verify

```bash
code --version
```

---

## 2. Install ESP-IDF Toolchain

There are two ways to install ESP-IDF. Choose the one that fits your workflow:

- **Option A** — Install via the VS Code extension (recommended for beginners)
- **Option B** — Install manually via command line

### Option A: Install via VS Code Extension (Recommended)

This is the easiest method. The extension handles downloading ESP-IDF, the toolchain, and all dependencies.

1. Open VS Code
2. Go to the **Extensions** panel (`Ctrl+Shift+X` / `Cmd+Shift+X`)
3. Search for **"ESP-IDF"** and install the extension by Espressif Systems
4. After installation, open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`) and run:
   ```
   ESP-IDF: Configure ESP-IDF Extension
   ```
5. Select **"Express"** setup mode
6. Choose **ESP-IDF v5.4** (or the latest v5.x release)
7. Leave the install directory as default and click **Install**
8. Wait for the download and setup to complete (this may take 15–30 minutes)

> The extension installs ESP-IDF, the Xtensa and RISC-V toolchains, Python dependencies, and OpenOCD — everything you need.

Once complete, you should see **"ESP-IDF"** in the VS Code status bar.

### Option B: Manual Installation (Command Line)

#### macOS / Linux

```bash
# Install system dependencies
# macOS:
xcode-select --install
brew install cmake ninja dfu-util

# Ubuntu/Debian:
sudo apt install -y git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
  dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git

# Run the install script (downloads toolchains)
cd esp-idf
./install.sh esp32s3

# Activate the environment (run this in every new terminal)
. ./export.sh
```

Add the following to your shell profile (`~/.bashrc`, `~/.zshrc`, etc.) for convenience:

```bash
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

Then simply run `get_idf` to activate ESP-IDF in any terminal.

#### Windows

1. Download the **ESP-IDF Tools Installer** from:
   [https://dl.espressif.com/dl/esp-idf/](https://dl.espressif.com/dl/esp-idf/)
2. Run the installer and select **ESP-IDF v5.4**
3. Choose **Full installation** (includes Git, Python, and all toolchains)
4. After installation, use the **ESP-IDF PowerShell** or **ESP-IDF Command Prompt** shortcuts from the Start Menu

### Verify ESP-IDF Installation

Open a terminal (or ESP-IDF terminal on Windows) and run:

```bash
idf.py --version
```

Expected output: `ESP-IDF v5.4` (or similar v5.x).

---

## 3. Install the VS Code Extension (Manual Install Users)

If you used **Option B** above, install the ESP-IDF extension separately:

1. Open VS Code
2. Go to **Extensions** (`Ctrl+Shift+X` / `Cmd+Shift+X`)
3. Search for **"ESP-IDF"** and install it
4. Open the Command Palette and run:
   ```
   ESP-IDF: Configure ESP-IDF Extension
   ```
5. Select **"Use existing setup"** and point it to your ESP-IDF directory (e.g., `~/esp/esp-idf`)

---

## 4. Configure the Project

### Clone and open

```bash
git clone https://github.com/mscy/ProjectCopilotDemoPi.git
cd ProjectCopilotDemoPi
code .
```

### Set the target

Open the VS Code Command Palette and run:

```
ESP-IDF: Set Espressif Device Target
```

Select **esp32s3**.

Or from the terminal:

```bash
idf.py set-target esp32s3
```

---

## 5. Build, Flash, and Monitor

### From VS Code

Use the icons in the bottom status bar (provided by the ESP-IDF extension):

| Icon | Action |
|------|--------|
| 🔨 Build | Compile the firmware |
| ⚡ Flash | Write firmware to the board |
| 📺 Monitor | Open serial monitor (115200 baud) |
| 🔥 Build + Flash + Monitor | All three in one click |

### From the terminal

```bash
# Build
idf.py build

# Flash (replace port as needed)
idf.py -p /dev/cu.usbmodem101 flash

# Monitor
idf.py -p /dev/cu.usbmodem101 monitor

# All in one
idf.py -p /dev/cu.usbmodem101 flash monitor
```

> **Tip:** Press `Ctrl+]` to exit the serial monitor.

---

## 6. Recommended VS Code Extensions

These optional extensions improve the development experience:

| Extension | Purpose |
|-----------|---------|
| **C/C++** (Microsoft) | IntelliSense, code navigation, debugging |
| **C/C++ Extension Pack** | Includes CMake Tools and other helpers |
| **Serial Monitor** | Built-in serial terminal |
| **GitLens** | Enhanced Git integration |

---

## 7. USB Driver Setup (if needed)

Most systems recognize the ESP32-S3 USB interface automatically. If your board is not detected:

### macOS
No additional drivers needed — the built-in USB CDC driver works out of the box.

### Linux
Add your user to the `dialout` group to access serial ports without `sudo`:

```bash
sudo usermod -aG dialout $USER
```

Log out and log back in for the change to take effect.

### Windows
If the board is not recognized, install the USB driver:
1. Download and install [Zadig](https://zadig.akeo.ie/) or the [CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) if your board uses a USB-to-UART bridge
2. For native USB (ESP32-S3 built-in USB), Windows 10/11 should detect it automatically

---

## 8. Troubleshooting

| Problem | Solution |
|---------|----------|
| `idf.py: command not found` | Run `. ~/esp/esp-idf/export.sh` (or `get_idf` alias) to activate ESP-IDF |
| VS Code extension can't find ESP-IDF | Re-run `ESP-IDF: Configure ESP-IDF Extension` and point to correct path |
| `Permission denied` on serial port (Linux) | Add user to `dialout` group (see section 7) |
| Build fails with Python errors | Ensure Python 3.8+ is installed; try `pip install --upgrade pip` |
| `No serial data received` during flash | Enter download mode manually: hold **BOOT**, press **RESET**, release **BOOT** |
| IntelliSense errors in VS Code | Run `ESP-IDF: Add vscode Configuration Folder` to generate `c_cpp_properties.json` |

---

## Next Steps

- Read **[AGENTS.md](../Firmware/AGENTS.md)** for firmware coding rules before writing any code
- Check the **[Firmware Plan](../Docs/CopilotDemoPi_firmware_plan.md)** for the development roadmap
- See **[FLASHING.md](../Firmware/FLASHING.md)** to flash a pre-built binary without building from source

---

*You're all set! Happy coding 🚀*
