# ST7789 / ST7789P3 LCD Driver Notes

This document summarizes how the 240×240 IPS panel (ST7789P3 controller) on
CopilotDemoPi is brought up from scratch on ESP32‑S3 using ESP‑IDF
`spi_master` only. Everything is implemented in
[main/drivers/lcd.c](main/drivers/lcd.c) /
[main/drivers/lcd.h](main/drivers/lcd.h), driven by the constants in
[main/config.h](main/config.h).

## 1. Hardware Spec — what the panel needs

The panel is a 2.2 240×240 round/square IPS module with an ST7789P3
controller on a 4‑wire SPI interface and no MISO. Relevant electrical and
protocol facts taken from the ST7789V/P3 datasheet and the module schematic:

| Signal | Direction | Notes |
| ------ | --------- | ----- |
| `SCLK` | MCU → LCD | SPI clock, sampled by the controller in 4‑wire mode |
| `SDA` (MOSI) | MCU → LCD | Command/data byte stream |
| `DC` | MCU → LCD | 0 = command, 1 = parameter / pixel data |
| `RST` | MCU → LCD | Active‑low hardware reset, ≥10 µs pulse, ≥120 ms recovery |
| `BL` | MCU → LCD | Backlight LED anode driver, active‑high |
| `CS` | tied low on the module | Permanently selected, no GPIO needed |

Required initialization sequence (from ST7789 datasheet §6 *Command List* and
§9 *Power-up Sequence*):

1. Hardware reset on `RST` — ≥10 µs low, then wait ≥120 ms.
2. `0x01 SWRESET` — software reset, wait ≥120 ms before any further command.
3. `0x11 SLPOUT` — leave sleep, wait ≥120 ms before issuing color/display
   commands.
4. `0x3A COLMOD` with parameter `0x55` — 16 bpp / RGB565 pixel format.
5. `0x36 MADCTL` — memory access control (orientation, RGB/BGR ordering).
6. `0x21 INVON` — display inversion ON. Required for normal IPS panels of
   this family; without it the image looks photo‑negative.
7. `0x13 NORON` — normal display mode.
8. `0x29 DISPON` — turn display ON.

Pixel addressing uses three commands per refresh: `0x2A CASET` (x window),
`0x2B RASET` (y window), `0x2C RAMWR` (start writing pixels). RGB565 is
transmitted **most‑significant byte first per pixel** — i.e. byte‑swapped
relative to the natural little‑endian `uint16_t` layout that C produces.

SPI mode: ST7789 latches data on rising clock with idle‑high clock, which is
SPI mode 3 (CPOL = 1, CPHA = 1). Datasheet says the controller can ingest
up to 62.5 MHz; 10 MHz is conservatively well within the device’s spec and
matches the trace lengths on the dev board.

## 2. Pin mapping in `config.h`

```c
#define LCD_SCLK_GPIO       6
#define LCD_MOSI_GPIO       7
#define LCD_DC_GPIO         14
#define LCD_RST_GPIO        21
#define LCD_BL_GPIO         17
#define LCD_CS_GPIO         (-1)   // tied low on the module
#define LCD_INVERT_COLOR    true
#define LCD_WIDTH           240
#define LCD_HEIGHT          240
```

Per the project rule (see [agents.md](agents.md)) the LCD owns its own SPI
bus (`SPI2_HOST`) — it does not share GPIOs with the SD card or the sensor
I²C bus, so no mutex is required.

## 3. Working configuration in code

The current configuration that produces a correct, stable image:

| What | Value | Why |
| ---- | ----- | --- |
| SPI host | `SPI2_HOST` | Dedicated to the LCD; SD card uses the other host |
| SPI mode | 3 | ST7789 requires CPOL = 1 / CPHA = 1 |
| Clock | 10 MHz (`LCD_SPI_CLOCK_HZ`) | Comfortable margin under panel max (62.5 MHz), tolerant of breadboard wiring |
| `cs_io_num` | −1 | CS is hard-wired to GND on the module |
| `queue_size` | 1 | Polling transfers, no deep queue needed |
| Transfer style | `spi_device_polling_transmit` | Lower latency than queued for short command/data writes |
| Pixel format | 16 bpp via `COLMOD = 0x55` | RGB565, smallest framebuffer cost |
| MADCTL | `0x00` | Default scan order (top‑to‑bottom, left‑to‑right, RGB) |
| Inversion | `INVON (0x21)` | This panel ships with inverted polarity expected |
| Pixel byte order | Big‑endian per pixel | Software byte‑swaps before DMA |
| Fill strip | 20 rows | DMA buffer = 20 × 240 × 2 = 9.6 KiB, fits internal DMA RAM |
| Backlight | GPIO17 driven high | No PWM in current build; on/off only |
| Reset | GPIO21 low 50 ms → high 120 ms | Meets datasheet minimums with margin |

`max_transfer_sz` is computed from the strip size:

```c
.max_transfer_sz = LCD_WIDTH * LCD_FILL_STRIP_ROWS * sizeof(uint16_t) + 64
```

so the SPI driver allocates DMA descriptors that exactly match the largest
flush we ever issue.

## 4. Mapping spec → code

This is how each datasheet requirement is realized in
[main/drivers/lcd.c](main/drivers/lcd.c):

### 4.1 GPIO setup
`gpio_config` in `lcd_init` configures `DC`, `RST`, `BL` as outputs with no
pull. `DC` is parked low (the SPI driver later drives it explicitly per
transfer), and `BL` is brought high before the panel finishes initializing
so the screen visibly recovers as soon as DRAM contents are valid.

### 4.2 SPI bus and device
- `spi_bus_initialize` is tolerant of `ESP_ERR_INVALID_STATE` — the SD card
  driver may have initialized this host earlier in some builds, so the LCD
  reuses it.
- `spi_device_interface_config_t` sets `mode = 3`, `clock_speed_hz =
  10 MHz`, `spics_io_num = -1` because CS is grounded.

### 4.3 Reset → SWRESET → SLPOUT
Mirrors the datasheet timing exactly: 50 ms low → 120 ms recovery →
`SWRESET` + 150 ms → `SLPOUT` + 120 ms. The 50 ms low pulse is much longer
than the 10 µs minimum, but it’s effectively free at boot and immune to
slow GPIO transitions on long jumper wires.

### 4.4 Color mode and orientation
`COLMOD = 0x55` selects RGB565. `MADCTL = 0x00` keeps native scan order
(no row/column flip, RGB filter order). Because the panel is square
(240 × 240) we do not need rotation in software.

### 4.5 Inversion
`LCD_INVERT_COLOR` is `true` so `INVON (0x21)` is issued. With this panel
batch the `INVOFF` path produces a film‑negative image, which is the giveaway
that you have the polarity flipped — the build flag is kept so a different
panel revision can disable inversion without code changes.

### 4.6 Window addressing
`lcd_set_window(x0, y0, x1, y1)` issues `CASET`, `RASET`, then `RAMWR`
exactly as required, packing each coordinate as big‑endian 16‑bit values.

### 4.7 Pixel byte order
RGB565 in C is stored low‑byte‑first, but ST7789 expects the high byte
first. Every pixel write byte‑swaps just before DMA:

```c
uint16_t sw = (uint16_t)((color << 8) | (color >> 8));
```

This is done once per DMA strip in `lcd_fill_rect` and per pixel in
`lcd_draw_bitmap`. It is also why the boot logo asset is generated as
RGB565 byte‑swapped.

### 4.8 Bulk pixel transfers
`lcd_fill_rect` allocates a single DMA‑capable strip buffer
(`MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`) of up to 20 rows, fills it once,
and then walks the rectangle vertically issuing
`set_window` + `RAMWR` + DMA write per strip. Re‑using the same buffer for
every strip keeps allocator pressure low and keeps fills fast enough for
the per‑detent UI updates that the encoder loop performs.

### 4.9 Public API

| Function | Maps to |
| -------- | ------- |
| `lcd_init` | reset → `SWRESET` → `SLPOUT` → `COLMOD` → `MADCTL` → `INVON` → `NORON` → `DISPON` |
| `lcd_fill_rect`, `lcd_fill` | `CASET` + `RASET` + `RAMWR` + DMA stream |
| `lcd_draw_bitmap` | same window setup, byte‑swapping caller pixels in strips |
| `lcd_on` / `lcd_off` | `0x29 DISPON` / `0x28 DISPOFF` |
| `lcd_self_test` | RGB primaries fill |
| `lcd_diagnostic_loop` | Continuous primaries + 8×8 checkerboard for bring‑up |
| `LCD_RGB565(r,g,b)` macro | Standard 5‑6‑5 packing |

## 5. Bring‑up war stories (lessons that shaped the current driver)

These are the gotchas that took multiple iterations and are now baked into
the working configuration:

1. **Inversion.** First image showed cyan‑tinted, washed‑out colors. Fix:
   issue `INVON (0x21)` in `lcd_init`. Captured behind `LCD_INVERT_COLOR`.
2. **Byte‑swapping per pixel.** Without the byte swap, blue and red were
   exchanged and gradients showed banding because the high/low byte split
   landed in the wrong color channel. Now done in both
   `lcd_fill_rect` and `lcd_draw_bitmap`.
3. **SPI mode.** Mode 0 produced random garbage on the screen; mode 3 is
   the correct choice for ST7789.
4. **CS pin.** The module ties CS to GND, so passing a real GPIO and
   driving it caused phantom transactions when the bus was shared. Setting
   `spics_io_num = -1` matches the hardware.
5. **DMA‑capable buffer.** Initial naive `malloc` returned PSRAM memory and
   `spi_device_polling_transmit` reported `ESP_ERR_INVALID_ARG`. Switching
   to `heap_caps_malloc(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)` solved it.
6. **Strip size.** 240×240×2 = 115 KiB doesn’t fit in a single internal DMA
   buffer; using a 20‑row strip keeps the working set under 10 KiB while
   still being big enough that the SPI clock dominates wall time, not the
   per‑transaction overhead.
7. **Reset timing.** Short reset pulses (≪1 ms) sometimes left the panel
   in an undefined state on cold boot. The 50 ms / 120 ms timings are well
   above the datasheet minimum and 100% reliable in practice.
8. **Backlight.** Driving `BL` *before* the init sequence completes means
   the user briefly sees the random‑power‑on RAM contents on cold boot.
   Keeping the panel in `SLPOUT` + black before `DISPON` is enough to
   suppress this without needing dedicated PWM dimming yet.

## 6. Future work

- PWM the backlight via LEDC for brightness control (pin `LCD_BL_GPIO`
  already supports it).
- Add hardware orientation switch via `MADCTL` if the enclosure is rotated.
- Consider switching from polling to queued transfers when the GUI starts
  redrawing larger regions; the current incremental redraw in
  [main/app/display.c](main/app/display.c) is already small enough that
  polling wins on latency.

---

## 7. Reproducibility kit (for a fresh agent / new build)

This section makes the document self-contained: a new agent should be able
to drop these snippets into a clean ESP-IDF project and reproduce the
working bring-up without re-deriving anything from the datasheet.

### 7.1 Toolchain & target

| Item | Value |
| ---- | ----- |
| Framework | ESP-IDF **v5.5.2** |
| Target | `esp32s3` (ESP32‑S3‑PICO‑1‑N8R8, 8 MB flash, 8 MB PSRAM) |
| Language | C only (see [agents.md](agents.md) — no Arduino / 3rd‑party libs) |
| Build system | CMake (ESP‑IDF default) |
| Component model | All driver code lives in `main/`, no extra components needed for the LCD |

`main/CMakeLists.txt` only needs the standard registration — no special
`REQUIRES` for the LCD driver beyond what ESP‑IDF already exposes:

```cmake
idf_component_register(
    SRCS "main.c" "drivers/lcd.c" # ... other driver/.c files
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc nvs_flash
)
```

(`driver` brings in `spi_master` and `gpio`; that's all the LCD needs.)

### 7.2 Panel identification

| Field | Value |
| ----- | ----- |
| Module | 2.2" 240×240 IPS, 4‑wire SPI, no MISO, no touch |
| Controller | Sitronix **ST7789P3** (P3 variant — accepts `INVON` polarity, V/V2 also work with the same sequence) |
| Resolution mode | 240×240 (RAM addressed 0..239 / 0..239, no offset needed for this module) |
| Voltage | VCC = 3.3 V, IO = 3.3 V (S3 is 3.3 V native — no level shifter) |

If you hit a different module, things to double‑check: RAM offsets
(some 240×240 modules need `x += 0`, others `x += 0, y += 0`; some
135×240 / 240×320 modules need non‑zero offsets in `CASET`/`RASET`),
and whether `INVON` or `INVOFF` produces correct colors (see §7.7 below).

### 7.3 Verbatim init sequence

Drop‑in equivalent of `lcd_init()` reset+command stream — execute in this
exact order, with these delays:

```text
RST low                     50 ms   (datasheet min: 10 µs; we use 50 ms for safety)
RST high                   120 ms   (datasheet min: 120 ms before any command)
CMD 0x01 SWRESET           150 ms   (datasheet min: 120 ms; sleep mode forced after)
CMD 0x11 SLPOUT            120 ms   (datasheet min: 120 ms before any color/display cmd)
CMD 0x3A COLMOD,  0x55       —      (16 bpp / RGB565)
CMD 0x36 MADCTL,  0x00       —      (default scan order, RGB filter)
CMD 0x21 INVON               —      (this panel batch — flip to 0x20 INVOFF if colors look photo‑negative)
CMD 0x13 NORON              10 ms
CMD 0x29 DISPON            120 ms
```

Each CMD byte is sent with `DC = 0`, each parameter byte with `DC = 1`.
Pixel data writes use `CASET (0x2A)` / `RASET (0x2B)` / `RAMWR (0x2C)`,
followed by RGB565 pixels **MSB first per pixel**.

### 7.4 MADCTL bit table (so orientation is changeable without the datasheet)

`MADCTL = 0x36` parameter byte:

| Bit | Name | Effect when set (1) |
| --- | ---- | ------------------- |
| D7  | MY   | Row address order: bottom→top |
| D6  | MX   | Column address order: right→left |
| D5  | MV   | Row/column exchange (transpose; needed for 90°/270°) |
| D4  | ML   | Vertical refresh order |
| D3  | RGB  | Pixel color filter: **0 = RGB, 1 = BGR** |
| D2  | MH   | Horizontal refresh order |
| D1‑D0 | — | Reserved, write 0 |

Common orientations for a 240×240 module:

| Orientation | MADCTL |
| ----------- | ------ |
| 0° (native, this build) | `0x00` |
| 90° CW   | `0x60`  (MV \| MX) |
| 180°     | `0xC0`  (MY \| MX) |
| 270° CW  | `0xA0`  (MV \| MY) |
| 0°, BGR panel | `0x08` |

If R and B look swapped, flip the `RGB` bit (`0x00` ↔ `0x08`). If the
image is mirrored, flip MX or MY. If text reads sideways, set MV.

### 7.5 COLMOD options

`COLMOD = 0x3A` parameter:

| Value | Format | Notes |
| ----- | ------ | ----- |
| `0x55` | 16 bpp / RGB565 | **Used here.** Smallest framebuffer, fastest fills. |
| `0x66` | 18 bpp / RGB666 | 3 bytes per pixel; some color banding goes away but DMA ×1.5. |
| `0x05` | 12 bpp / RGB444 | Two pixels share three bytes; rarely worth it. |

### 7.6 SPI / DMA configuration rationale

| Setting | Value | Why this exact value |
| ------- | ----- | -------------------- |
| Host | `SPI2_HOST` | ESP32‑S3: `SPI2_HOST` is the GPSPI host generally available; `SPI3_HOST` works too. |
| Mode | **3** (CPOL=1, CPHA=1) | ST7789 latches MOSI on rising edge with idle‑high clock. |
| Clock | 10 MHz | Datasheet allows up to 62.5 MHz; 10 MHz is reliable on jumper wires and leaves margin for poor signal integrity. Bump later if needed. |
| `spics_io_num` | `-1` | CS is tied to GND on the module — driving a phantom CS GPIO causes spurious transactions if the bus is shared. |
| `queue_size` | 1 | We use polling transfers, not queued, so the queue is unused. |
| Transfer | `spi_device_polling_transmit` | Lower latency than queued for short command/data writes. |
| Strip rows | 20 | DMA buffer = 20·240·2 = 9.6 KiB → fits comfortably in internal RAM. |
| `max_transfer_sz` | `LCD_WIDTH * LCD_FILL_STRIP_ROWS * 2 + 64` | Tells the SPI driver how big a DMA descriptor chain to allocate. |
| Allocator | `heap_caps_malloc(MALLOC_CAP_DMA \| MALLOC_CAP_INTERNAL)` | **Critical.** PSRAM (`MALLOC_CAP_SPIRAM`) is *not* DMA‑addressable on ESP32‑S3 for SPI master; using it returns intermittent `ESP_ERR_INVALID_ARG`. |

### 7.7 Pixel byte‑order rationale

ST7789 in 16‑bit/pixel SPI mode latches **the high byte of each RGB565
pixel first** off SDA. C on a little‑endian core stores `uint16_t` low
byte first, so a raw `(uint16_t *)pixels` DMA write transmits the bytes
in the wrong order — you'll see garbled color channels (R/B swap is the
most visible symptom). Fix once at the buffer:

```c
uint16_t sw = (uint16_t)((color << 8) | (color >> 8));
```

This is per‑pixel work; for a 240×240 fill that's 57 600 swaps, far
cheaper than the SPI transfer that follows, so just do it inline.

### 7.8 GPIO / DC ordering rules

- `DC` must be at the correct level **before** the first SPI bit of each
  transaction. With `spi_device_polling_transmit` we set `DC` with
  `gpio_set_level()` immediately before each transmit and that's enough
  because polling blocks the calling task — no race.
- If you ever switch to queued transfers, you must move the `DC` toggle
  into a `pre_cb` (`spi_device_interface_config_t::pre_cb`) so it
  happens at the right moment in the SPI driver's dispatch.
- `RST` is a plain GPIO; pulse low, wait, pulse high, wait. No SPI
  involvement.
- `BL` is plain GPIO‑on for now. The panel doesn't care about backlight
  state during init; you can turn it on before or after `DISPON`. If
  you don't want a flash of garbage RAM contents at cold boot, do
  `DISPOFF`/black fill first, then turn `BL` on.

### 7.9 Bus‑sharing rules

This project gives the LCD its own dedicated SPI host (`SPI2_HOST`) and
puts the SD card on `SPI3_HOST`. If you must share a host:

- Use a real CS GPIO for the LCD (not −1) and use `spi_bus_add_device`
  with `cs_io_num = LCD_CS_GPIO`.
- Don't share the same DMA buffer across devices on the same host
  without serializing access.
- Polling transmit will block the bus for the duration of the transfer;
  that is fine for the small writes the LCD does but be aware of it.

### 7.10 Verification checklist (in order)

When bringing up a new board, expect to see:

1. Backlight visibly on within ~200 ms of power. If not → wrong `BL`
   pin or wrong polarity (some modules are active‑low).
2. `lcd_self_test()` shows pure RED → GREEN → BLUE → dark gray.
3. If image is **photo‑negative** (whites are black, colors inverted)
   → flip `LCD_INVERT_COLOR` (use `INVOFF (0x20)` instead of `INVON`).
4. If **R and B are swapped** → flip MADCTL `RGB` bit (`0x00` ↔ `0x08`).
5. If image is **mirrored horizontally** → flip MADCTL MX (`bit 6`).
6. If image is **upside down** → flip MADCTL MY (`bit 7`) or use 180° = `0xC0`.
7. If image is **rotated 90°** → set MADCTL MV (`bit 5`).
8. If colors look right but image **shifts by some pixels** → check
   `CASET`/`RASET` offsets; some modules need `x += offset_x`.
9. If you see **horizontal tearing during fills** → DMA buffer in PSRAM;
   force `MALLOC_CAP_INTERNAL`.
10. If **only the first strip looks right and the rest is garbage** →
    `max_transfer_sz` too small for your strip.

### 7.11 Symptom → cause → fix table

| Symptom | Cause | Fix |
| ------- | ----- | --- |
| Black screen, backlight on | Init never reached `DISPON`, or stuck at `SWRESET` | Check `RST` GPIO, ensure 120 ms wait, confirm SPI mode 3 |
| Random snow / garbage | Wrong SPI mode | Set `mode = 3` |
| Photo‑negative colors | Panel polarity opposite of `INVOFF` default | Issue `INVON (0x21)` |
| R and B swapped | MADCTL RGB bit wrong | Flip MADCTL bit 3 |
| Banded gradients / wrong hues | Pixel byte‑order wrong | Byte‑swap each pixel before DMA |
| `spi_device_*` returns `ESP_ERR_INVALID_ARG` intermittently | Buffer in PSRAM | `heap_caps_malloc(MALLOC_CAP_DMA\|MALLOC_CAP_INTERNAL)` |
| Image works for first 20 rows then garbage | `max_transfer_sz` too small | `width * strip_rows * 2 + 64` |
| Image works on warm reset, fails on cold boot | `RST` not held long enough | 50 ms low, 120 ms high recovery |
| Phantom transactions when SD card is active | `spics_io_num` set on a CS‑grounded module | Set `spics_io_num = -1` |
| Pixels offset by N at one edge | Module needs RAM offset | Add per‑module `OFFSET_X` / `OFFSET_Y` to `lcd_set_window` |

### 7.12 Verbatim `lcd_init()` (drop-in reference)

This is the actual init that boots reliably on this hardware. Keep it as
the starting point; the only thing you typically need to change for a
different module is `MADCTL` and possibly `LCD_INVERT_COLOR`.

```c
esp_err_t lcd_init(void)
{
    if (s_initialized) return ESP_OK;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LCD_DC_GPIO) |
                        (1ULL << LCD_RST_GPIO) |
                        (1ULL << LCD_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio");

    gpio_set_level((gpio_num_t)LCD_DC_GPIO, 0);
    gpio_set_level((gpio_num_t)LCD_BL_GPIO, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num   = LCD_SCLK_GPIO,
        .mosi_io_num   = LCD_MOSI_GPIO,
        .miso_io_num   = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_FILL_STRIP_ROWS * sizeof(uint16_t) + 64,
    };
    esp_err_t err = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode           = 3,
        .spics_io_num   = -1,
        .queue_size     = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(LCD_HOST, &devcfg, &s_spi),
                        TAG, "add device");

    gpio_set_level((gpio_num_t)LCD_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level((gpio_num_t)LCD_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x01), TAG, "SWRESET");
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x11), TAG, "SLPOUT");
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x3A), TAG, "COLMOD");
    ESP_RETURN_ON_ERROR(lcd_write_u8(0x55),  TAG, "COLMOD param");

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x36), TAG, "MADCTL");
    ESP_RETURN_ON_ERROR(lcd_write_u8(0x00),  TAG, "MADCTL param");

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x21), TAG, "INVON");

    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x13), TAG, "NORON");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(lcd_write_cmd(0x29), TAG, "DISPON");
    vTaskDelay(pdMS_TO_TICKS(120));

    s_initialized = true;
    return ESP_OK;
}
```

### 7.13 Project rule reminders (from agents.md)

A reproducing agent must obey:

- C only, ESP‑IDF v5.x APIs only, no Arduino/PlatformIO/Adafruit libs.
- Write the LCD driver from scratch — no `lvgl_esp32_drivers`, no
  `TFT_eSPI`, no `Adafruit_ST7789`.
- `snake_case` symbols, module-prefixed (`lcd_init`, `lcd_fill`, …).
- Errors via `esp_err_t`, log via `ESP_LOGI/W/E`, never `printf`.
- Hardware constants live in `config.h`; no magic numbers in driver code.

---

*Generated as a working summary of the ST7789P3 LCD bring‑up for
CopilotDemoPi.*

