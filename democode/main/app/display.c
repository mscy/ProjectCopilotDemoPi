#include "display.h"
#include "lcd.h"
#include "config.h"
#include "mp3_player.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "display";

typedef enum {
    SCR_NONE = 0,
    SCR_BOOT,
    SCR_BME,
    SCR_BMI,
    SCR_PLAYER,
    SCR_BALANCE,
    SCR_I2C_SCAN,
    SCR_M5_I2C_LCD,
    SCR_MODE,
} screen_t;

static screen_t s_current_screen = SCR_NONE;
static char s_last_bme[5][16];
static char s_last_bmi[6][16];
static char s_last_player[2][16];
static char s_last_bal[1][16];
static char s_last_i2c[1][16];
static char s_last_m5[7][16];

static const uint8_t *display_glyph(char ch)
{
    static const uint8_t blank[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t n0[7] = { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E };
    static const uint8_t n1[7] = { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E };
    static const uint8_t n2[7] = { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F };
    static const uint8_t n3[7] = { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E };
    static const uint8_t n4[7] = { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 };
    static const uint8_t n5[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E };
    static const uint8_t n6[7] = { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E };
    static const uint8_t n7[7] = { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
    static const uint8_t n8[7] = { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E };
    static const uint8_t n9[7] = { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C };
    static const uint8_t dot[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C };
    static const uint8_t minus[7] = { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 };
    static const uint8_t plus[7] = { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 };
    static const uint8_t pct[7] = { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 };
    static const uint8_t a[7] = { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    static const uint8_t b[7] = { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E };
    static const uint8_t c[7] = { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E };
    static const uint8_t d[7] = { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
    static const uint8_t e[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
    static const uint8_t f[7] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
    static const uint8_t g[7] = { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F };
    static const uint8_t h[7] = { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    static const uint8_t i[7] = { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F };
    static const uint8_t k[7] = { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
    static const uint8_t l[7] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
    static const uint8_t m[7] = { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
    static const uint8_t n[7] = { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
    static const uint8_t o[7] = { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    static const uint8_t p[7] = { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 };
    static const uint8_t q[7] = { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D };
    static const uint8_t r[7] = { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
    static const uint8_t s[7] = { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
    static const uint8_t t[7] = { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    static const uint8_t u[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    static const uint8_t v[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };
    static const uint8_t x[7] = { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 };
    static const uint8_t y[7] = { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 };
    static const uint8_t z[7] = { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F };

    if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
    switch (ch) {
    case '0': return n0;
    case '1': return n1;
    case '2': return n2;
    case '3': return n3;
    case '4': return n4;
    case '5': return n5;
    case '6': return n6;
    case '7': return n7;
    case '8': return n8;
    case '9': return n9;
    case '.': return dot;
    case '-': return minus;
    case '+': return plus;
    case '%': return pct;
    case 'A': return a;
    case 'B': return b;

    case 'C': return c;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'G': return g;
    case 'H': return h;
    case 'I': return i;
    case 'K': return k;
    case 'L': return l;
    case 'M': return m;
    case 'N': return n;
    case 'O': return o;
    case 'P': return p;
    case 'Q': return q;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'X': return x;
    case 'Y': return y;
    case 'Z': return z;
    default: return blank;
    }
}

static void display_draw_text(int x, int y, const char *text, int scale, uint16_t color)
{
    while (*text) {
        if (*text == ' ') {
            x += 4 * scale;
            text++;
            continue;
        }

        const uint8_t *glyph = display_glyph(*text);
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (glyph[row] & (1 << (4 - col))) {
                    lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
        text++;
    }
}

static int display_text_width(const char *text, int scale)
{
    int width = 0;
    while (*text) {
        width += (*text == ' ') ? 4 * scale : 6 * scale;
        text++;
    }
    return width > 0 ? width - scale : 0;
}

// Draw text only if it changed since last frame; clear cell with bg first.
static void update_value_cell(int x, int y, int w, int h, uint16_t bg,
                              const char *new_text, char *cache, uint16_t color)
{
    if (strncmp(new_text, cache, 16) == 0) return;
    lcd_fill_rect(x, y, w, h, bg);
    display_draw_text(x, y, new_text, 2, color);
    strncpy(cache, new_text, 15);
    cache[15] = 0;
}

static void display_invalidate_cache(void)
{
    for (int i = 0; i < 5; i++) s_last_bme[i][0] = 0;
    for (int i = 0; i < 6; i++) s_last_bmi[i][0] = 0;
    for (int i = 0; i < 2; i++) s_last_player[i][0] = 0;
    for (int i = 0; i < 1; i++) s_last_bal[i][0] = 0;
    for (int i = 0; i < 7; i++) s_last_m5[i][0] = 0;
}

esp_err_t display_boot_screen(void)
{
    const uint16_t bg = LCD_RGB565(8, 12, 20);
    const uint16_t blue = LCD_RGB565(0, 120, 255);
    const uint16_t green = LCD_RGB565(0, 210, 150);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t dim = LCD_RGB565(70, 85, 105);

    lcd_fill(bg);

    lcd_fill_rect(58, 34, 54, 54, blue);
    lcd_fill_rect(128, 34, 54, 54, green);
    lcd_fill_rect(74, 50, 22, 22, bg);
    lcd_fill_rect(144, 50, 22, 22, bg);
    lcd_fill_rect(106, 55, 28, 12, white);
    lcd_fill_rect(108, 67, 24, 8, white);

    const int top_scale = 3;
    const char *top = "COPILOT";
    int top_x = (LCD_WIDTH - display_text_width(top, top_scale)) / 2;
    display_draw_text(top_x, 112, top, top_scale, white);

    const int bottom_scale = 3;
    const char *bottom = "DEMOPI";
    int bottom_x = (LCD_WIDTH - display_text_width(bottom, bottom_scale)) / 2;
    display_draw_text(bottom_x, 150, bottom, bottom_scale, green);

    lcd_fill_rect(52, 196, 136, 3, dim);
    lcd_fill_rect(82, 206, 76, 2, blue);
    s_current_screen = SCR_BOOT;
    display_invalidate_cache();
    ESP_LOGI(TAG, "boot screen drawn");
    return ESP_OK;
}

// Value-cell layout (must stay in sync between chrome and update paths).
#define BME_VAL_X        126
#define BME_PRESS_VAL_X  110
#define BME_VAL_W        108
#define BME_IAQ_VAL_X    102
#define BME_IAQ_VAL_W    132
#define BME_PRESS_VAL_W  124
#define BME_VAL_H        14

static void display_draw_bme_chrome(void)
{
    const uint16_t bg = LCD_RGB565(6, 10, 16);
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);
    const uint16_t green = LCD_RGB565(0, 220, 150);
    const uint16_t amber = LCD_RGB565(255, 190, 60);

    lcd_fill(bg);
    display_draw_text(60, 10, "BME690", 3, white);
    lcd_fill_rect(14, 46, 102, 4, cyan);
    lcd_fill_rect(124, 47, 102, 2, panel);

    lcd_fill_rect(12, 56, 216, 28, panel);
    display_draw_text(22, 64, "TEMP", 2, cyan);

    lcd_fill_rect(12, 88, 216, 28, panel);
    display_draw_text(22, 96, "HUM", 2, green);

    lcd_fill_rect(12, 120, 216, 28, panel);
    display_draw_text(22, 128, "PRESS", 2, amber);

    lcd_fill_rect(12, 152, 216, 28, panel);
    display_draw_text(22, 160, "VOC", 2, green);

    lcd_fill_rect(12, 184, 216, 28, panel);
    display_draw_text(22, 192, "IAQ", 2, cyan);

    display_draw_text(40, 218, "Environment", 2, green);
}

esp_err_t display_show_bme690(float temperature_c, float humidity_pct, float pressure_hpa,
                              float gas_resistance_ohm, int iaq, uint8_t iaq_accuracy,
                              bool iaq_from_bsec2)
{
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);

    if (s_current_screen != SCR_BME) {
        display_draw_bme_chrome();
        s_current_screen = SCR_BME;
        display_invalidate_cache();
    }

    char line[16];

    snprintf(line, sizeof(line), "%.1fC", (double)temperature_c);
    update_value_cell(BME_VAL_X, 64, BME_VAL_W, BME_VAL_H, panel, line, s_last_bme[0], white);

    snprintf(line, sizeof(line), "%.1f%%", (double)humidity_pct);
    update_value_cell(BME_VAL_X, 96, BME_VAL_W, BME_VAL_H, panel, line, s_last_bme[1], white);

    snprintf(line, sizeof(line), "%.1fHPA", (double)pressure_hpa);
    update_value_cell(BME_PRESS_VAL_X, 128, BME_PRESS_VAL_W, BME_VAL_H, panel, line, s_last_bme[2], white);

    float voc_kohm = gas_resistance_ohm / 1000.0f;
    if (voc_kohm > 999.9f) {
        snprintf(line, sizeof(line), "HIGH");
    } else {
        snprintf(line, sizeof(line), "%.1fK", (double)voc_kohm);
    }
    update_value_cell(BME_VAL_X, 160, BME_VAL_W, BME_VAL_H, panel, line, s_last_bme[3], white);

    if (iaq < 0) iaq = 0;
    if (iaq > 500) iaq = 500;
    if (iaq_from_bsec2) {
        // Compact form for narrow 5x7 font: e.g. 87A2
        snprintf(line, sizeof(line), "%dA%u", iaq, (unsigned)iaq_accuracy);
    } else {
        // Fallback estimate marker: e.g. 142E
        snprintf(line, sizeof(line), "%dE", iaq);
    }
    update_value_cell(BME_IAQ_VAL_X, 192, BME_IAQ_VAL_W, BME_VAL_H, panel, line, s_last_bme[4], white);

    return ESP_OK;
}

#define BMI_VAL_X 92
#define BMI_VAL_W 130
#define BMI_VAL_H 14

static void display_draw_bmi_chrome(void)
{
    const uint16_t bg = LCD_RGB565(6, 10, 16);
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t pink = LCD_RGB565(255, 120, 190);
    const uint16_t amber = LCD_RGB565(255, 190, 60);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);

    lcd_fill(bg);
    display_draw_text(60, 10, "BMI270", 3, white);

    lcd_fill_rect(14, 46, 102, 2, panel);
    lcd_fill_rect(124, 46, 102, 4, pink);

    lcd_fill_rect(14, 62, 212, 82, panel);
    display_draw_text(26, 74, "ACC", 2, pink);

    lcd_fill_rect(14, 154, 212, 58, panel);
    display_draw_text(26, 168, "GYRO", 2, amber);

    display_draw_text(96, 218, "IMU", 2, cyan);
}

esp_err_t display_show_bmi270(float ax_mps2, float ay_mps2, float az_mps2,
                              float gx_dps, float gy_dps, float gz_dps)
{
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);

    if (s_current_screen != SCR_BMI) {
        display_draw_bmi_chrome();
        s_current_screen = SCR_BMI;
        display_invalidate_cache();
    }

    char line[16];

    snprintf(line, sizeof(line), "X%.1f", (double)ax_mps2);
    update_value_cell(BMI_VAL_X, 74, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[0], white);
    snprintf(line, sizeof(line), "Y%.1f", (double)ay_mps2);
    update_value_cell(BMI_VAL_X, 96, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[1], white);
    snprintf(line, sizeof(line), "Z%.1f", (double)az_mps2);
    update_value_cell(BMI_VAL_X, 118, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[2], white);

    snprintf(line, sizeof(line), "X%.1f", (double)gx_dps);
    update_value_cell(BMI_VAL_X, 160, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[3], white);
    snprintf(line, sizeof(line), "Y%.1f", (double)gy_dps);
    update_value_cell(BMI_VAL_X, 178, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[4], white);
    snprintf(line, sizeof(line), "Z%.1f", (double)gz_dps);
    update_value_cell(BMI_VAL_X, 196, BMI_VAL_W, BMI_VAL_H, panel, line, s_last_bmi[5], white);

    return ESP_OK;
}

static void display_draw_player_chrome(void)
{
    const uint16_t bg = LCD_RGB565(4, 8, 18);
    const uint16_t panel = LCD_RGB565(10, 18, 34);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t blue = LCD_RGB565(40, 120, 255);

    lcd_fill(bg);
    display_draw_text(38, 14, "Copilot Player", 2, white);
    lcd_fill_rect(12, 62, 216, 2, panel);
    lcd_fill_rect(18, 70, 204, 116, panel);
    lcd_fill_rect(18, 188, 204, 2, blue);
    display_draw_text(18, 198, "VOL", 2, blue);
    display_draw_text(150, 198, "TAB 4", 2, blue);
}

esp_err_t display_show_player(bool playing, uint8_t volume_pct,
                              const uint8_t *bars, size_t n_bars)
{
    const uint16_t bg = LCD_RGB565(4, 8, 18);
    const uint16_t panel = LCD_RGB565(10, 18, 34);
    const uint16_t grid = LCD_RGB565(25, 50, 80);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);
    const uint16_t blue = LCD_RGB565(40, 120, 255);
    const uint16_t green = LCD_RGB565(0, 220, 150);
    const uint16_t amber = LCD_RGB565(255, 190, 60);
    const uint16_t red = LCD_RGB565(255, 90, 90);

    if (s_current_screen != SCR_PLAYER) {
        display_draw_player_chrome();
        s_current_screen = SCR_PLAYER;
        display_invalidate_cache();
    }

    lcd_fill_rect(18, 70, 204, 116, panel);
    
    // Draw center line (zero crossing)
    lcd_fill_rect(22, 118, 196, 1, grid);
    
    // Draw waveform (resample to visible graph width to avoid right-edge squeeze)
    const int wave_samples = 240;
    const int graph_x = 22;
    const int graph_w = 196;
    uint8_t waveform[wave_samples];
    mp3_player_get_waveform(waveform, wave_samples);

    int prev_y = 118;
    for (int i = 0; i < graph_w; i++) {
        int x = graph_x + i;
        int src_idx = (i * wave_samples) / graph_w;
        if (src_idx >= wave_samples) src_idx = wave_samples - 1;
        // Convert waveform sample (0-255) to Y coordinate
        // Sample 128 = middle (prev_y), 0 = top, 255 = bottom
        int val = waveform[src_idx];
        int offset = (int)(val - 128);  // -128..127
        int h = (offset * 46) / 128;    // Scale to fit in 92 pixels
        int y = 118 - h;
        
        // Draw vertical line at this X position
        uint8_t level = val < 128 ? (128 - val) : (val - 128);
        uint16_t color;
        if (level < 20) {
            color = cyan;
        } else if (level < 50) {
            color = green;
        } else if (level < 90) {
            color = amber;
        } else {
            color = red;
        }
        
        // Draw a small vertical segment and connect to previous
        if (i > 0) {
            int min_y = prev_y < y ? prev_y : y;
            int max_y = prev_y > y ? prev_y : y;
            lcd_fill_rect(x, min_y, 1, max_y - min_y + 1, color);
        } else {
            lcd_fill_rect(x, y, 1, 1, color);
        }
        prev_y = y;
    }

    char line[16];
    snprintf(line, sizeof(line), "%s", playing ? "READY" : "IDLE");
    update_value_cell(18, 72, 92, 14, panel, line, s_last_player[0], playing ? green : amber);

    snprintf(line, sizeof(line), "%u%%", (unsigned)volume_pct);
    update_value_cell(72, 198, 64, 14, bg, line, s_last_player[1], blue);
    return ESP_OK;
}

// ===========================================================================
// I2C scanner tab — shows scan progress bar and list of found addresses
// ===========================================================================

static void display_draw_i2c_chrome(void)
{
    const uint16_t bg = LCD_RGB565(6, 10, 16);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);

    lcd_fill(bg);
    display_draw_text(48, 10, "I2C SCAN", 3, white);
    lcd_fill_rect(14, 46, 212, 4, cyan);
}

esp_err_t display_show_i2c_scan(uint8_t progress, const uint8_t *addrs, size_t count)
{
    const uint16_t bg = LCD_RGB565(6, 10, 16);
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);
    const uint16_t green = LCD_RGB565(0, 220, 150);
    const uint16_t amber = LCD_RGB565(255, 190, 60);

    static uint8_t s_last_progress = 0xFF;
    static size_t  s_last_count = (size_t)-1;
    static uint8_t s_last_addrs[16] = {0};
    static bool    s_last_summary_drawn = false;

    bool first_draw = false;
    if (s_current_screen != SCR_I2C_SCAN) {
        display_draw_i2c_chrome();
        s_current_screen = SCR_I2C_SCAN;
        display_invalidate_cache();
        s_last_progress = 0xFF;
        s_last_count = (size_t)-1;
        s_last_summary_drawn = false;
        first_draw = true;
    }

    // Progress bar (skip if unchanged) — moved to bottom of screen
    if (first_draw || progress != s_last_progress) {
        // Bar: x=18..222, y=208, h=8
        int bar_w = (int)((uint32_t)progress * 204 / 100);
        lcd_fill_rect(18, 208, 204, 8, panel);
        if (bar_w > 0) {
            lcd_fill_rect(18, 208, bar_w, 8, cyan);
        }
        // Percent text below the bar, centered, size 2
        char ptext[16];
        snprintf(ptext, sizeof(ptext), "%u%%", (unsigned)progress);
        int pw = display_text_width(ptext, 2);
        lcd_fill_rect(60, 222, 120, 14, bg);
        display_draw_text((LCD_WIDTH - pw) / 2, 222, ptext, 2, white);
        s_last_progress = progress;
    }

    // Device grid: redraw only when the address list changed
    bool grid_changed = first_draw || count != s_last_count;
    if (!grid_changed) {
        size_t check = count > 16 ? 16 : count;
        for (size_t i = 0; i < check; i++) {
            if (addrs[i] != s_last_addrs[i]) { grid_changed = true; break; }
        }
    }
    if (grid_changed) {
        // Grid area y=80..200 (above the progress bar at y=208)
        lcd_fill_rect(12, 80, 216, 122, bg);
        size_t show = count > 8 ? 8 : count;
        for (size_t i = 0; i < show; i++) {
            int col = (i < 4) ? 0 : 1;
            int row = (int)(i % 4);
            int x = 18 + col * 110;
            int y = 82 + row * 28;

            lcd_fill_rect(x, y, 100, 22, panel);
            char addr_str[16];
            snprintf(addr_str, sizeof(addr_str), "0x%02X", addrs[i]);
            display_draw_text(x + 6, y + 4, addr_str, 2, green);
        }
        if (count > 8) {
            display_draw_text(80, 196, "+MORE", 1, amber);
        }
        s_last_count = count;
        size_t copy = count > 16 ? 16 : count;
        for (size_t i = 0; i < copy; i++) s_last_addrs[i] = addrs[i];
        s_last_summary_drawn = false;
    }

    // Summary line — drawn just under the title once scan finishes
    if (progress >= 100 && (!s_last_summary_drawn || grid_changed)) {
        char summary[16];
        snprintf(summary, sizeof(summary), "%u FOUND", (unsigned)count);
        int sw = display_text_width(summary, 2);
        lcd_fill_rect(12, 56, 216, 16, bg);
        display_draw_text((LCD_WIDTH - sw) / 2, 56, summary, 2, count > 0 ? green : amber);
        s_last_summary_drawn = true;
    }

    return ESP_OK;
}

static void display_draw_m5_i2c_lcd_chrome(void)
{
    const uint16_t bg = LCD_RGB565(8, 10, 18);
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);
    const uint16_t amber = LCD_RGB565(255, 190, 60);

    lcd_fill(bg);
    display_draw_text(22, 10, "M5 I2C LCD", 3, white);
    lcd_fill_rect(14, 46, 212, 4, cyan);

    lcd_fill_rect(12, 62, 216, 28, panel);
    display_draw_text(20, 70, "ADDR", 2, amber);

    lcd_fill_rect(12, 94, 216, 28, panel);
    display_draw_text(20, 102, "ID", 2, cyan);

    lcd_fill_rect(12, 126, 216, 28, panel);
    display_draw_text(20, 134, "STAT", 2, amber);

    lcd_fill_rect(12, 158, 216, 28, panel);
    display_draw_text(20, 166, "BRT", 2, cyan);

    lcd_fill_rect(12, 190, 216, 28, panel);
    display_draw_text(20, 198, "FRAME", 2, amber);
}

esp_err_t display_show_m5_i2c_lcd(bool online, uint8_t addr, const uint8_t id[4],
                                  uint8_t brightness, uint16_t frame, esp_err_t last_err)
{
    const uint16_t panel = LCD_RGB565(16, 24, 36);
    const uint16_t white = LCD_RGB565(245, 250, 255);
    const uint16_t cyan = LCD_RGB565(40, 210, 255);
    const uint16_t green = LCD_RGB565(0, 220, 150);
    const uint16_t red = LCD_RGB565(255, 90, 90);

    if (s_current_screen != SCR_M5_I2C_LCD) {
        display_draw_m5_i2c_lcd_chrome();
        s_current_screen = SCR_M5_I2C_LCD;
        display_invalidate_cache();
    }

    char line[16];

    snprintf(line, sizeof(line), "0X%02X", (unsigned)addr);
    update_value_cell(108, 70, 112, 14, panel, line, s_last_m5[0], white);

    uint8_t id0 = id ? id[0] : 0;
    uint8_t id1 = id ? id[1] : 0;
    uint8_t id2 = id ? id[2] : 0;
    uint8_t id3 = id ? id[3] : 0;
    snprintf(line, sizeof(line), "%02X%02X%02X%02X", id0, id1, id2, id3);
    update_value_cell(108, 102, 112, 14, panel, line, s_last_m5[1], cyan);

    snprintf(line, sizeof(line), "%s", online ? "ONLINE" : "OFFLINE");
    update_value_cell(108, 134, 112, 14, panel, line, s_last_m5[2], online ? green : red);

    snprintf(line, sizeof(line), "%u", (unsigned)brightness);
    update_value_cell(108, 166, 112, 14, panel, line, s_last_m5[3], cyan);

    snprintf(line, sizeof(line), "%u", (unsigned)frame);
    update_value_cell(108, 198, 112, 14, panel, line, s_last_m5[4], white);

    if (last_err == ESP_OK) {
        snprintf(line, sizeof(line), "OK");
    } else {
        snprintf(line, sizeof(line), "E%X", (unsigned)last_err);
    }
    update_value_cell(24, 222, 90, 14, LCD_RGB565(8, 10, 18), line, s_last_m5[5], white);

    snprintf(line, sizeof(line), "0X%02X", (unsigned)addr);
    update_value_cell(126, 222, 90, 14, LCD_RGB565(8, 10, 18), line, s_last_m5[6], cyan);
    return ESP_OK;
}

// ===========================================================================
// HUD tab — fighter-pilot Attitude Director Indicator (ADI / artificial
// horizon) rendered into a 200×200 canvas pushed with one lcd_draw_bitmap().
//
// Coordinate convention:
//   roll  = rotation around the board's long (X) axis, positive = right wing down
//   pitch = rotation around the board's short (Y) axis, positive = nose down
//
// The canvas is divided sky (above horizon) / ground (below horizon).
// A pitch ladder (lines at ±10°, ±20°, ±30°) rotates with the roll.
// A fixed aircraft symbol and centre dot sit on top.
// Roll arc + tick marks at ±10/20/30/60/90° sit at the top of the circle.
// ===========================================================================

#define HUD_W      200
#define HUD_H      200
#define HUD_X       20     // canvas offset on the 240×240 screen
#define HUD_Y       20

// Reuse BAL_* names in s_last_bal so nothing else needs touching.
#define BAL_W   HUD_W
#define BAL_H   HUD_H

static uint16_t s_bal_canvas[HUD_W * HUD_H];

// Low-pass filtered roll & pitch (radians).
static float s_hud_roll  = 0.0f;
static float s_hud_pitch = 0.0f;

// ---------- rasteriser helpers (all clip to the canvas) ----------

static inline void hud_set(int x, int y, uint16_t c)
{
    if ((unsigned)x < HUD_W && (unsigned)y < HUD_H)
        s_bal_canvas[y * HUD_W + x] = c;
}

// Thin Bresenham line (no thickness variant needed here).
static void hud_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx+dy;
    for (;;) {
        hud_set(x0, y0, c);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2>=dy){ err+=dy; x0+=sx; }
        if (e2<=dx){ err+=dx; y0+=sy; }
    }
}

// Thick line (t=1..3).
static void hud_thick_line(int x0, int y0, int x1, int y1, int t, uint16_t c)
{
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx+dy;
    int h = t/2;
    for (;;) {
        for (int oy=-h; oy<=h; oy++)
            for (int ox=-h; ox<=h; ox++)
                hud_set(x0+ox, y0+oy, c);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2>=dy){ err+=dy; x0+=sx; }
        if (e2<=dx){ err+=dx; y0+=sy; }
    }
}

static void hud_disk(int cx, int cy, int r, uint16_t c)
{
    int r2 = r*r;
    for (int y=-r; y<=r; y++)
        for (int x=-r; x<=r; x++)
            if (x*x+y*y<=r2) hud_set(cx+x, cy+y, c);
}

// Fill a horizontal span (clipped).
// Draw the sky/ground split rotated by roll, pitched by pitch.
// The horizon is a line through (cx,cy) with normal pointing "up" in screen space.
// px_per_deg: how many pixels one degree of pitch shifts the horizon vertically
//             (in the roll-rotated frame).
#define PX_PER_DEG  3.5f

static void hud_draw_horizon(int cx, int cy, float roll_rad, float pitch_rad,
                             uint16_t sky_c, uint16_t ground_c, uint16_t horizon_c)
{
    // Horizon offset in pixels (positive pitch → nose up → horizon moves down on screen).
    float pitch_px = pitch_rad * (180.0f / 3.14159f) * PX_PER_DEG;

    float cr = cosf(roll_rad);
    float sr = sinf(roll_rad);

    // For each row in the canvas, determine which side of the horizon it's on.
    for (int y = 0; y < HUD_H; y++) {
        for (int x = 0; x < HUD_W; x++) {
            // Position relative to canvas centre.
            float rx = (float)(x - cx);
            float ry = (float)(y - cy);
            // Rotate by -roll to align with aircraft frame.
            float ay = -sr * rx + cr * ry;
            // ay > pitch_px means below horizon.
            if (ay > pitch_px)
                s_bal_canvas[y * HUD_W + x] = ground_c;
            else
                s_bal_canvas[y * HUD_W + x] = sky_c;
        }
    }

    // Horizon line — two endpoints far enough to cross the full canvas.
    int hlen = 150;
    int hx0 = cx - (int)(cr * hlen);
    int hy0 = cy - (int)(-sr * hlen) + (int)(pitch_px * cr);  // projected back
    int hx1 = cx + (int)(cr * hlen);
    int hy1 = cy + (int)(-sr * hlen) - (int)(pitch_px * cr);
    // Simpler: just extend along the roll direction.
    // The horizon crosses the centre-offset point (cx + sr*pitch_px, cy - cr*pitch_px)
    // and is perpendicular to the gravity direction in screen space.
    float ox = sr * pitch_px;   // horizon centre x offset
    float oy = -cr * pitch_px;  // horizon centre y offset
    hx0 = cx + (int)(ox - cr * hlen);
    hy0 = cy + (int)(oy + sr * hlen);
    hx1 = cx + (int)(ox + cr * hlen);
    hy1 = cy + (int)(oy - sr * hlen);
    hud_thick_line(hx0, hy0, hx1, hy1, 2, horizon_c);
}

// Draw one pitch-ladder line at `pitch_offset_deg` degrees from current pitch.
// Lines alternate length for major (multiples of 20°) / minor (10°) ticks.
static void hud_pitch_line(int cx, int cy, float roll_rad, float pitch_rad,
                           float pitch_offset_deg, uint16_t c)
{
    float pitch_px = (pitch_rad * (180.0f / 3.14159f) - pitch_offset_deg) * PX_PER_DEG;
    float cr = cosf(roll_rad);
    float sr = sinf(roll_rad);
    // Centre of this ladder line in screen space.
    float lx = cx + sr * pitch_px;
    float ly = cy - cr * pitch_px;

    int major = ((int)fabsf(pitch_offset_deg) % 20 == 0) ? 1 : 0;
    int half_len = major ? 30 : 18;

    int x0 = (int)(lx - cr * half_len);
    int y0 = (int)(ly + sr * half_len);
    int x1 = (int)(lx + cr * half_len);
    int y1 = (int)(ly - sr * half_len);
    hud_thick_line(x0, y0, x1, y1, 1, c);

    // Short end-caps pointing toward horizon.
    int cap = 5;
    float nx = -sr, ny = -cr; // points toward sky
    if (pitch_offset_deg > 0) { nx = -nx; ny = -ny; } // flip for below horizon
    hud_line(x0, y0, x0+(int)(nx*cap), y0+(int)(ny*cap), c);
    hud_line(x1, y1, x1+(int)(nx*cap), y1+(int)(ny*cap), c);
}

// Draw roll arc + tick marks (arc radius R around canvas centre).
static void hud_roll_arc(int cx, int cy, float roll_rad, uint16_t c, uint16_t ptr_c)
{
    const int R = 82;
    // Arc from -90° to +90° (top of the display).
    // Draw arc by sampling every 2°.
    float prev_x = 0, prev_y = 0;
    int first = 1;
    for (int a = -90; a <= 90; a += 2) {
        float ang = a * (3.14159f / 180.0f) - 3.14159f / 2.0f; // 0 at top
        float px = cx + R * cosf(ang);
        float py = cy + R * sinf(ang);
        if (!first) hud_line((int)prev_x, (int)prev_y, (int)px, (int)py, c);
        prev_x = px; prev_y = py;
        first = 0;
    }

    // Tick marks at ±10, ±20, ±30, ±60, ±90.
    static const int ticks[] = { -90,-60,-30,-20,-10, 10, 20, 30, 60, 90 };
    for (int ti = 0; ti < 10; ti++) {
        int td = ticks[ti];
        int tlen = (abs(td) % 30 == 0) ? 10 : 6;
        float ang = td * (3.14159f / 180.0f) - 3.14159f / 2.0f;
        int ix = cx + (int)(R * cosf(ang));
        int iy = cy + (int)(R * sinf(ang));
        int ox = cx + (int)((R - tlen) * cosf(ang));
        int oy = cy + (int)((R - tlen) * sinf(ang));
        hud_thick_line(ix, iy, ox, oy, 1, c);
    }

    // Roll pointer triangle at the current roll angle (inverted triangle pointing down
    // toward centre when wings-level, rotates with roll).
    float ptr_ang = -roll_rad - 3.14159f / 2.0f;
    int tip_x  = cx + (int)((R - 14) * cosf(ptr_ang));
    int tip_y  = cy + (int)((R - 14) * sinf(ptr_ang));
    int base_x = cx + (int)((R + 2)  * cosf(ptr_ang));
    int base_y = cy + (int)((R + 2)  * sinf(ptr_ang));
    // Perpendicular to pointer direction for the base of the triangle.
    float perp_x = -sinf(ptr_ang), perp_y = cosf(ptr_ang);
    int bl_x = base_x + (int)(perp_x * 5);
    int bl_y = base_y + (int)(perp_y * 5);
    int br_x = base_x - (int)(perp_x * 5);
    int br_y = base_y - (int)(perp_y * 5);
    hud_thick_line(tip_x, tip_y, bl_x, bl_y, 1, ptr_c);
    hud_thick_line(tip_x, tip_y, br_x, br_y, 1, ptr_c);
    hud_thick_line(bl_x, bl_y, br_x, br_y, 1, ptr_c);
}

// Aircraft symbol: fixed W-shape (wings + fuselage cross).
static void hud_aircraft(int cx, int cy, uint16_t c, uint16_t dot_c)
{
    // Left wing bar.
    hud_thick_line(cx - 40, cy, cx - 10, cy, 2, c);
    // Right wing bar.
    hud_thick_line(cx + 10, cy, cx + 40, cy, 2, c);
    // Short vertical fuselage tick.
    hud_thick_line(cx, cy - 6, cx, cy + 6, 2, c);
    // Notch at wing roots (bent-up tips).
    hud_thick_line(cx - 10, cy, cx - 16, cy - 6, 2, c);
    hud_thick_line(cx + 10, cy, cx + 16, cy - 6, 2, c);
    // Centre dot (flight path marker).
    hud_disk(cx, cy, 3, dot_c);
}

// One small digit (0-9) drawn 3×5 into the canvas.
static void hud_digit(int x, int y, int d, uint16_t c)
{
    static const uint8_t seg[10][5] = {
        {0x7,0x5,0x5,0x5,0x7},{0x2,0x2,0x2,0x2,0x2},{0x7,0x1,0x7,0x4,0x7},
        {0x7,0x1,0x7,0x1,0x7},{0x5,0x5,0x7,0x1,0x1},{0x7,0x4,0x7,0x1,0x7},
        {0x7,0x4,0x7,0x5,0x7},{0x7,0x1,0x1,0x1,0x1},{0x7,0x5,0x7,0x5,0x7},
        {0x7,0x5,0x7,0x1,0x7}
    };
    if (d<0||d>9) return;
    for (int row=0;row<5;row++)
        for (int col=0;col<3;col++)
            if (seg[d][row]&(1<<(2-col)))
                hud_set(x+col, y+row, c);
}

// Print a signed integer up to ±999 at (x,y) in small pixels.
static void hud_int(int x, int y, int val, uint16_t c)
{
    if (val < 0) {
        hud_set(x, y+2, c); hud_set(x+1, y+2, c); // minus sign
        x += 3;
        val = -val;
    }
    int hundreds = val / 100;
    int tens     = (val % 100) / 10;
    int ones     = val % 10;
    if (hundreds) { hud_digit(x, y, hundreds, c); x += 4; }
    if (tens || hundreds) { hud_digit(x, y, tens, c); x += 4; }
    hud_digit(x, y, ones, c);
}

static void display_draw_hud_chrome(void)
{
    // Minimalist chrome — just black background, the canvas area speaks for
    // itself. A thin amber border around the ADI circle is drawn per-frame.
    lcd_fill(LCD_RGB565(0, 0, 0));
}

esp_err_t display_show_balance(float ax_mps2, float ay_mps2, float az_mps2)
{
    if (s_current_screen != SCR_BALANCE) {
        display_draw_hud_chrome();
        s_current_screen = SCR_BALANCE;
        display_invalidate_cache();
        s_hud_roll  = 0.0f;
        s_hud_pitch = 0.0f;
    }

    // ---- Derive roll & pitch from accelerometer. ----
    // With the board held flat (az ~ -g, ax~ay~0): roll=pitch=0.
    // roll:  right edge down → positive roll.   atan2(ax, -az)
    // pitch: nose (USB) down  → positive pitch.  atan2(ay, -az)
    // BMI270 on this board has Z pointing down, so az ≈ +9.81 when flat.
    // atan2(ax, az) → 0 when flat, increases as the board tilts.
    float az = az_mps2;
    if (fabsf(az) < 0.1f) az = (az >= 0.0f) ? 0.1f : -0.1f;
    float roll_inst  = atan2f( ay_mps2, az);
    float pitch_inst = atan2f( ax_mps2, az);

    // Clamp to ±85° to avoid atan2 wrap-around artefacts near vertical.
    const float max_rad = 85.0f * (3.14159f / 180.0f);
    if (roll_inst  >  max_rad) roll_inst  =  max_rad;
    if (roll_inst  < -max_rad) roll_inst  = -max_rad;
    if (pitch_inst >  max_rad) pitch_inst =  max_rad;
    if (pitch_inst < -max_rad) pitch_inst = -max_rad;

    // Low-pass: α=0.25 → smooth but responsive.
    s_hud_roll  = 0.75f * s_hud_roll  + 0.25f * roll_inst;
    s_hud_pitch = 0.75f * s_hud_pitch + 0.25f * pitch_inst;

    float roll  = s_hud_roll;
    float pitch = s_hud_pitch;

    // ---- Palette ----
    const uint16_t sky_c     = LCD_RGB565(20,  80, 200);  // deep blue sky
    const uint16_t gnd_c     = LCD_RGB565(130,  70,  20);  // earthy brown
    const uint16_t horizon_c = LCD_RGB565(255, 255, 255);  // white horizon line
    const uint16_t ladder_c  = LCD_RGB565(255, 255, 180);  // yellow-white ladder
    const uint16_t arc_c     = LCD_RGB565(200, 200, 200);  // grey arc/ticks
    const uint16_t ptr_c     = LCD_RGB565(255, 210,  30);  // amber pointer
    const uint16_t ac_c      = LCD_RGB565(255, 220,  40);  // amber aircraft symbol
    const uint16_t dot_c     = LCD_RGB565(255,  80,  80);  // red centre dot
    const uint16_t border_c  = LCD_RGB565(100, 100, 100);  // border

    int cx = HUD_W / 2;
    int cy = HUD_H / 2;

    // ---- 1. Sky/ground fill (rotated + pitched) ----
    hud_draw_horizon(cx, cy, roll, pitch, sky_c, gnd_c, horizon_c);

    // ---- 2. Circular mask: clear pixels outside the ADI circle to black ----
    int R_mask = 92;
    for (int y = 0; y < HUD_H; y++) {
        for (int x = 0; x < HUD_W; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy > R_mask*R_mask)
                s_bal_canvas[y * HUD_W + x] = LCD_RGB565(0, 0, 0);
        }
    }

    // ---- 3. Pitch ladder: lines at ±10, ±20, ±30 degrees ----
    for (int pd = -30; pd <= 30; pd += 10) {
        if (pd == 0) continue;
        hud_pitch_line(cx, cy, roll, pitch, (float)pd, ladder_c);
    }

    // ---- 4. Roll arc + tick marks ----
    hud_roll_arc(cx, cy, roll, arc_c, ptr_c);

    // ---- 5. Fixed aircraft symbol (always centred, not rotated) ----
    hud_aircraft(cx, cy, ac_c, dot_c);

    // ---- 6. Thin circle border ----
    for (int a = 0; a < 360; a++) {
        float ang = a * (3.14159f / 180.0f);
        hud_set(cx + (int)(R_mask * cosf(ang)), cy + (int)(R_mask * sinf(ang)), border_c);
    }

    // ---- 7. Digital readouts at corners (inside the circle) ----
    int roll_deg  = (int)(roll  * 180.0f / 3.14159f);
    int pitch_deg = (int)(pitch * 180.0f / 3.14159f);
    // Left: roll, right: pitch (small pixel numbers).
    hud_set(22, 88, 0); hud_set(23,88,0); // clear area not needed; just write
    hud_int(15,  90, roll_deg,  LCD_RGB565(255,200, 60));
    hud_int(145, 90, pitch_deg, LCD_RGB565( 60,255,180));

    // Labels "R" and "P" above the numbers (single-pixel dots as shorthand).
    // Instead: draw tiny "R" and "P" using hud_digit-sized marks.
    hud_set(14, 86, LCD_RGB565(255,200,60));
    hud_set(144,86, LCD_RGB565(60,255,180));

    // ---- 8. Push to LCD ----
    esp_err_t err = lcd_draw_bitmap(HUD_X, HUD_Y, HUD_W, HUD_H, s_bal_canvas);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HUD flush failed: %s", esp_err_to_name(err));
        return err;
    }

    // ---- 9. Text readout below the ADI (outside canvas, on black bg) ----
    char line[16];
    snprintf(line, sizeof(line), "R%+d P%+d", roll_deg, pitch_deg);
    const uint16_t bg_c  = LCD_RGB565(0, 0, 0);
    const uint16_t txt_c = LCD_RGB565(255, 210, 30);
    update_value_cell(30, 223, 180, 14, bg_c, line, s_last_bal[0], txt_c);

    return ESP_OK;
}

esp_err_t display_show_mode(const char *name)
{
    uint16_t color = LCD_RGB565(20, 20, 20);
    if (name && name[0] == 'B') color = LCD_RGB565(0, 60, 200);
    else if (name && name[0] == 'S') color = LCD_RGB565(0, 180, 60);
    else if (name && name[0] == 'R') color = LCD_RGB565(220, 180, 0);
    lcd_fill(color);
    s_current_screen = SCR_MODE;
    display_invalidate_cache();
    ESP_LOGI(TAG, "screen=%s", name ? name : "?");
    return ESP_OK;
}

