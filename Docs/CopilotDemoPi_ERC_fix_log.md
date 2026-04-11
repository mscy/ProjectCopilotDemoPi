# CopilotDemoPi — ERC Fix Log

> AI-assisted ERC debugging session | April 11, 2026

---

## ERC Progression

| Round | Errors | Warnings | Key Fix |
|-------|--------|----------|---------|
| ERC 1 | 2 | 95 | Initial run |
| ERC 2 | 1 | 95 | Fixed PM_VOut / +3.3V short |
| ERC 3 | 2 | 34 | Fixed off-grid (Align to Grid) |
| ERC 4 | 1 | 7 | Fixed GPIO46_Ext / +3.3V short |
| ERC 5 | 1 | 7 | Found GPIO35_SD_MOSI / +3.3V short |
| **ERC 6** | **1** | **6** | **Fixed GPIO35 short. Clean.** |

Final 1 Error = PWR_FLAG false positive (excluded).

---

## Issues Found & Fixed

### 1. PM_VOut shorted to +3.3V ⚡ (Critical)

**Problem:** BQ24072 OUT (PM_VOut, 3.0-5.0V) was directly connected to the +3.3V rail. This would bypass the TPS63001 voltage regulator and potentially feed 5V directly into the ESP32 (max 3.6V), destroying the chip.

**Fix:** Separated PM_VOut and +3.3V networks. PM_VOut now correctly feeds into TPS63001 input, which outputs regulated 3.3V.

**How AI helped:** Detected `[multiple_net_names]: Both +3.3V and PM_VOut are attached to the same items` in ERC report and flagged it as a critical issue.

---

### 2. GPIO46_Ext shorted to +3.3V (Medium)

**Problem:** The GPIO46 expansion pin header (J6) was accidentally connected to the +3.3V power rail. This would permanently drive GPIO46 high, which is a strapping pin — potentially affecting boot behavior.

**Fix:** Removed the stray wire connecting GPIO46_Ext to +3.3V.

**How AI helped:** Detected `[multiple_net_names]: Both +3.3V and GPIO46_Ext are attached to the same items` and identified the root cause.

---

### 3. GPIO35_SD_MOSI shorted to +3.3V ⚡ (Critical)

**Problem:** The MicroSD MOSI data line (GPIO35) crossed over a +3.3V power trace in the schematic, creating an unintended junction. This shorted the SPI data line to 3.3V, which would prevent MicroSD communication entirely.

**Fix:** Deleted the crossing wire junction, rerouted GPIO35_SD_MOSI to avoid the +3.3V trace near the MicroSD connector.

**How AI helped:** Detected `[multiple_net_names]: Both +3.3V and GPIO35_SD_MOSI are attached to the same items`, then analyzed the schematic screenshot to pinpoint the exact crossing point.

---

### 4. ~80 Off-Grid Warnings (Cosmetic)

**Problem:** Many component pins and wire endpoints were not aligned to the schematic grid, causing 80+ `endpoint_off_grid` warnings.

**Fix:** Used KiCad's **Edit → Cleanup Schematic** and **Align to Grid** to snap all elements to the 50mil grid in one operation.

**How AI helped:** Suggested the batch fix method instead of manually adjusting each element.

---

### 5. PWR_FLAG False Positives (Benign)

**Problem:** KiCad flagged "Power output pins connected to Power output pins" for:
- BQ24072 OUT ↔ PWR_FLAG
- TPS63001 VOUT ↔ PWR_FLAG

**Fix:** Excluded in ERC (right-click → Exclude). These are false positives — PWR_FLAG is intentionally placed to tell KiCad the net is driven.

---

### 6. Known Acceptable Warnings

| Warning | Reason | Action |
|---------|--------|--------|
| `+3.3V and VDD3P3 same net` | Same rail, different name | OK, KiCad uses +3.3V |
| `GND and Bat- same net` | Battery negative = ground | OK |
| `Bidirectional pin connected to Power output` | BME690/BMI270 SDO pin tied to GND for I2C address select | OK, by design |
| `No-connect pin connected` | ESP32-S3-PICO-1 NC pins (53, 54) | OK, internal pins |

---

## Final ERC Status

```
Errors:   1 (excluded — PWR_FLAG false positive)
Warnings: 6 (all reviewed and accepted)
Result:   ✅ PASS — Ready for PCB layout
```

---

## Key Takeaway

3 out of 6 issues were **accidental shorts caused by overlapping wires in the schematic**. AI detected all of them through ERC report analysis before any physical board was manufactured, potentially saving hundreds of yuan in respins.

---

*CopilotDemoPi ERC Fix Log | AI-Powered Hardware Design Demo*
*by Cysoft & Chac 🐕 | April 2026*
