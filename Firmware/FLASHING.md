# Flashing Guide — CopilotDemoPi

This guide explains how to flash the prebuilt firmware image onto an
**ESP32-S3-PICO-1-N8R8** based CopilotDemoPi board.

---

## 1. Prerequisites

### Hardware
- CopilotDemoPi board
- USB-C cable (data-capable, not power-only)
- Host PC (macOS / Linux / Windows)

### Software
Install **esptool.py** (the only tool required for flashing a prebuilt image).

```bash
# Recommended — install via pip
pip install --upgrade esptool
```

Verify the installation:

```bash
esptool.py version
# Expected: esptool.py v4.x or newer
```

> If you have ESP-IDF installed, `esptool.py` is already available after
> sourcing `export.sh`.

---

## 2. Firmware File

A single merged binary is provided:

| File | Offset | Size |
|------|--------|------|
| `build/ProjectDemoPi_init-merged.bin` | `0x0` | ~1.1 MB |

This image already contains:
- Second-stage bootloader (`0x0`)
- Partition table (`0x8000`)
- Application (`0x20000`)

You only need to write this **one file** to flash offset `0x0`.

---

## 3. Identify the Serial Port

Plug the board into the host PC via USB-C and locate the serial device.

| OS | Typical port |
|----|--------------|
| macOS | `/dev/cu.usbmodem*` or `/dev/cu.usbserial-*` |
| Linux | `/dev/ttyUSB0` or `/dev/ttyACM0` |
| Windows | `COM3`, `COM4`, ... |

Quick checks:

```bash
# macOS / Linux
ls /dev/cu.* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

> If no port appears, double-check that the USB cable supports data
> transfer and that your OS recognizes the on-chip USB-Serial device.

---

## 4. Enter Download Mode

The CopilotDemoPi auto-resets into download mode via the USB CDC interface.
If automatic entry fails, manually force it:

1. Hold **BOOT** (GPIO0).
2. Press and release **RESET** (or power-cycle).
3. Release **BOOT**.

The chip is now waiting for esptool to write flash.

---

## 5. Flash the Firmware

Run **one** of the commands below from the project root.

### One-shot flash (recommended)

```bash
esptool.py --chip esp32s3 \
  --port /dev/cu.usbmodem101 \
  --baud 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode dio \
  --flash_size 8MB \
  --flash_freq 80m \
  0x0 build/ProjectDemoPi_init-merged.bin
```

Replace `--port` with the actual port from step 3.

Expected output:

```
Connecting....
Chip is ESP32-S3 (...)
...
Writing at 0x000xxxxx... (100 %)
Wrote 1110160 bytes ... in xx.x seconds
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

### Full-erase flash (use if upgrading from a very different build)

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem101 erase_flash
```

Then re-run the `write_flash` command above.

---

## 6. Verify

After flashing, the board reboots automatically. To watch the boot log:

```bash
# Using ESP-IDF monitor
idf.py -p /dev/cu.usbmodem101 monitor

# Or any serial terminal at 115200 baud
screen /dev/cu.usbmodem101 115200
# (exit: Ctrl-A then K)
```

You should see:

```
I (xxx) demopi: CopilotDemoPi — fw <version>
I (xxx) demopi: ESP32-S3, 2 cores, rev x
...
```

The LCD will show the boot logo, run a brief LED test, then enter the
sensor-tab UI.

---

## 7. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Failed to connect to ESP32-S3: No serial data received` | Re-enter download mode manually (BOOT + RESET). |
| Wrong port / permission denied (Linux) | `sudo usermod -aG dialout $USER` and re-login, or run with `sudo`. |
| Flash succeeds but board reboots in a loop | Run `erase_flash` then re-flash. Confirm 8MB flash size. |
| Garbled monitor output | Make sure baud rate is **115200**. |
| LCD stays dark | Check battery voltage / USB power; backlight is off below `BATT_VOLTAGE_SHUT` (3.30 V). |

---

## 8. Reverting to a Development Build

To rebuild from source instead of using the merged binary:

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

---

*End of guide.*
