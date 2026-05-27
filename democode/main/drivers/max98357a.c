#include "max98357a.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "audio";

static i2s_chan_handle_t s_tx = NULL;
static bool   s_enabled = false;
static uint8_t s_volume = 70;        // target volume set by user
static uint8_t s_volume_actual = 70;  // smoothed volume applied to samples
static SemaphoreHandle_t s_i2s_mutex = NULL;

static esp_err_t audio_ensure_enabled(void)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;
    if (s_enabled) return ESP_OK;
    esp_err_t err = i2s_channel_enable(s_tx);
    if (err == ESP_OK) s_enabled = true;
    return err;
}

static esp_err_t audio_write_silence(size_t frames)
{
    enum { CHUNK = 256 };
    int16_t zero[CHUNK * AUDIO_CHANNELS] = { 0 };
    while (frames) {
        size_t f = frames > CHUNK ? CHUNK : frames;
        size_t w;
        esp_err_t err = i2s_channel_write(s_tx, zero,
                                          f * AUDIO_CHANNELS * sizeof(int16_t),
                                          &w, portMAX_DELAY);
        if (err != ESP_OK) return err;
        frames -= f;
    }
    return ESP_OK;
}

static esp_err_t audio_stop(void)
{
    if (!s_tx || !s_enabled) return ESP_OK;
    xSemaphoreTake(s_i2s_mutex, portMAX_DELAY);
    // Flush DMA buffers so they don't loop the last samples.
    audio_write_silence(2048);
    esp_err_t err = i2s_channel_disable(s_tx);
    if (err == ESP_OK) s_enabled = false;
    xSemaphoreGive(s_i2s_mutex);
    return err;
}

esp_err_t audio_stream_stop(void)
{
    return audio_stop();
}

esp_err_t audio_init(void)
{
    if (s_tx) return ESP_OK;
    if (!s_i2s_mutex) {
        s_i2s_mutex = xSemaphoreCreateMutex();
        if (!s_i2s_mutex) return ESP_ERR_NO_MEM;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (err != ESP_OK) { ESP_LOGE(TAG, "new channel: %s", esp_err_to_name(err)); return err; }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_LRCLK_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "init std: %s", esp_err_to_name(err)); return err; }
    // Channel stays disabled until the first audio_write / chime / etc.
    s_enabled = false;
    ESP_LOGI(TAG, "I2S ready %dHz/%dbit (idle)", AUDIO_SAMPLE_RATE, AUDIO_BITS);
    return ESP_OK;
}

esp_err_t audio_set_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    s_volume = pct;
    return ESP_OK;
}

uint8_t audio_get_volume(void) { return s_volume; }

esp_err_t audio_write(const int16_t *samples, size_t n_frames, size_t *written)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_i2s_mutex, portMAX_DELAY);
    esp_err_t err = audio_ensure_enabled();
    if (err != ESP_OK) { xSemaphoreGive(s_i2s_mutex); return err; }
    size_t bytes = n_frames * AUDIO_CHANNELS * sizeof(int16_t);
    // Apply software volume in-place would mutate caller buffer; copy small chunk.
    // For brevity, scale into a stack buffer (caller chunks should be reasonable).
    enum { CHUNK_FRAMES = 256 };
    int16_t buf[CHUNK_FRAMES * AUDIO_CHANNELS];
    size_t total = 0;
    while (n_frames) {
        // Slew-rate limit: move actual volume toward target by at most 2 per chunk
        if (s_volume_actual < s_volume) {
            uint8_t step = s_volume - s_volume_actual;
            s_volume_actual += (step > 2) ? 2 : step;
        } else if (s_volume_actual > s_volume) {
            uint8_t step = s_volume_actual - s_volume;
            s_volume_actual -= (step > 2) ? 2 : step;
        }
        size_t f = n_frames > CHUNK_FRAMES ? CHUNK_FRAMES : n_frames;
        for (size_t i = 0; i < f * AUDIO_CHANNELS; i++) {
            int32_t v = (int32_t)samples[i] * s_volume_actual / 100;
            if (v > INT16_MAX) v = INT16_MAX;
            if (v < INT16_MIN) v = INT16_MIN;
            buf[i] = (int16_t)v;
        }
        size_t w = 0;
        esp_err_t werr = i2s_channel_write(s_tx, buf, f * AUDIO_CHANNELS * sizeof(int16_t),
                                          &w, portMAX_DELAY);
        if (werr != ESP_OK) return werr;
        total += w;
        samples += f * AUDIO_CHANNELS;
        n_frames -= f;
    }
    if (written) *written = total;
    (void)bytes;
    xSemaphoreGive(s_i2s_mutex);
    return ESP_OK;
}

esp_err_t audio_self_test(void)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;
    // 200ms 1kHz tone
    enum { FRAMES = AUDIO_SAMPLE_RATE / 5 };
    static int16_t tone[FRAMES * 2];
    for (int i = 0; i < FRAMES; i++) {
        float s = sinf(2.0f * 3.14159265f * 1000.0f * i / AUDIO_SAMPLE_RATE);
        int16_t v = (int16_t)(s * 8000);
        tone[2*i] = v;
        tone[2*i+1] = v;
    }
    size_t w;
    esp_err_t err = audio_write(tone, FRAMES, &w);
    ESP_LOGI(TAG, "self-test %s (wrote %u bytes)", err == ESP_OK ? "PASS" : "FAIL", (unsigned)w);
    return err;
}

esp_err_t audio_play_chime(void)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    // Three ascending notes: C5, E5, G5 (a major triad), 120 ms each.
    static const float NOTES_HZ[] = { 523.25f, 659.25f, 783.99f };
    enum { NOTE_MS = 120, NOTE_FRAMES = AUDIO_SAMPLE_RATE * NOTE_MS / 1000 };

    // Stack buffer is too large for a 4 KB task — allocate once on heap.
    int16_t *buf = malloc(NOTE_FRAMES * 2 * sizeof(int16_t));
    if (!buf) return ESP_ERR_NO_MEM;

    const float TWO_PI = 2.0f * 3.14159265f;
    const int FADE = AUDIO_SAMPLE_RATE / 200; // 5 ms attack/release

    for (size_t n = 0; n < sizeof(NOTES_HZ) / sizeof(NOTES_HZ[0]); n++) {
        float freq = NOTES_HZ[n];
        for (int i = 0; i < NOTE_FRAMES; i++) {
            float env = 1.0f;
            if (i < FADE)                 env = (float)i / FADE;
            else if (i > NOTE_FRAMES - FADE) env = (float)(NOTE_FRAMES - i) / FADE;
            float s = sinf(TWO_PI * freq * i / AUDIO_SAMPLE_RATE) * env;
            int16_t v = (int16_t)(s * 12000);
            buf[2*i]     = v;
            buf[2*i + 1] = v;
        }
        size_t w;
        esp_err_t err = audio_write(buf, NOTE_FRAMES, &w);
        if (err != ESP_OK) { free(buf); return err; }
    }

    free(buf);
    audio_stop();   // flush DMA + disable channel so nothing loops in idle
    ESP_LOGI(TAG, "startup chime played");
    return ESP_OK;
}

esp_err_t audio_play_click(void)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    enum { CLICK_MS = 28, CLICK_FRAMES = AUDIO_SAMPLE_RATE * CLICK_MS / 1000 };
    int16_t *buf = malloc(CLICK_FRAMES * AUDIO_CHANNELS * sizeof(int16_t));
    if (!buf) return ESP_ERR_NO_MEM;

    const float two_pi = 2.0f * 3.14159265f;
    const float freq = 1800.0f;

    for (int i = 0; i < CLICK_FRAMES; i++) {
        float t = (float)i / (float)CLICK_FRAMES;
        float env = (1.0f - t) * (1.0f - t);
        float s = sinf(two_pi * freq * i / AUDIO_SAMPLE_RATE) * env;
        int16_t v = (int16_t)(s * 7000);
        buf[2 * i] = v;
        buf[2 * i + 1] = v;
    }

    size_t written;
    esp_err_t err = audio_write(buf, CLICK_FRAMES, &written);
    free(buf);
    if (err != ESP_OK) return err;
    audio_stop();
    return ESP_OK;
}

esp_err_t audio_play_tick(void)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    // Short, sharp "tock": 18 ms, two-tone (1.5 kHz + 750 Hz), exponential decay.
    enum { TICK_MS = 18, TICK_FRAMES = AUDIO_SAMPLE_RATE * TICK_MS / 1000 };
    int16_t *buf = malloc(TICK_FRAMES * AUDIO_CHANNELS * sizeof(int16_t));
    if (!buf) return ESP_ERR_NO_MEM;

    const float two_pi = 2.0f * 3.14159265f;
    const float f1 = 1500.0f;
    const float f2 = 750.0f;
    const float decay = 4.0f / TICK_FRAMES;

    for (int i = 0; i < TICK_FRAMES; i++) {
        float env = expf(-decay * i);
        // Hard attack: full amplitude immediately, decay fast.
        float s = (sinf(two_pi * f1 * i / AUDIO_SAMPLE_RATE) +
                   0.6f * sinf(two_pi * f2 * i / AUDIO_SAMPLE_RATE)) * env;
        // Clamp to avoid clipping with the 2-tone sum.
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t v = (int16_t)(s * 18000);
        buf[2 * i]     = v;
        buf[2 * i + 1] = v;
    }

    size_t written;
    esp_err_t err = audio_write(buf, TICK_FRAMES, &written);
    free(buf);
    if (err != ESP_OK) return err;
    // DMA = 6 * 240 = 1440 frames; if we leave it running it will loop the last
    // buffer. Flush enough silence to cover all DMA descriptors, then disable
    // the channel so it's idle until the next tick.
    audio_stop();
    return ESP_OK;
}

// Internal helper: play a sequence of pitched notes, blocking.
static esp_err_t play_note_sequence(const float *freqs, int n_notes,
                                    int note_ms, float amp_scale)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;
    int frames = AUDIO_SAMPLE_RATE * note_ms / 1000;
    int16_t *buf = malloc(frames * AUDIO_CHANNELS * sizeof(int16_t));
    if (!buf) return ESP_ERR_NO_MEM;

    const float two_pi = 2.0f * 3.14159265f;
    const int fade = AUDIO_SAMPLE_RATE / 250; // ~4 ms attack/release

    for (int n = 0; n < n_notes; n++) {
        float freq = freqs[n];
        for (int i = 0; i < frames; i++) {
            float env = 1.0f;
            if (i < fade)              env = (float)i / fade;
            else if (i > frames - fade) env = (float)(frames - i) / fade;
            float s = sinf(two_pi * freq * i / AUDIO_SAMPLE_RATE) * env;
            int16_t v = (int16_t)(s * amp_scale);
            buf[2*i]     = v;
            buf[2*i + 1] = v;
        }
        size_t w;
        esp_err_t err = audio_write(buf, frames, &w);
        if (err != ESP_OK) { free(buf); return err; }
    }
    free(buf);
    audio_stop();
    return ESP_OK;
}

esp_err_t audio_play_scan_start(void)
{
    // Quick rising two-tone "boop-beep": A5 → E6, 70 ms each.
    static const float NOTES[] = { 880.0f, 1318.51f };
    return play_note_sequence(NOTES, 2, 70, 11000.0f);
}

esp_err_t audio_play_scan_done(void)
{
    // "Ding-dong": E6 → C5, 110 + 180 ms (descending major third + octave).
    static const float NOTES[] = { 1318.51f, 523.25f };
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    const int durations_ms[] = { 110, 200 };
    const float two_pi = 2.0f * 3.14159265f;

    int max_frames = AUDIO_SAMPLE_RATE * 200 / 1000;
    int16_t *buf = malloc(max_frames * AUDIO_CHANNELS * sizeof(int16_t));
    if (!buf) return ESP_ERR_NO_MEM;

    for (int n = 0; n < 2; n++) {
        int frames = AUDIO_SAMPLE_RATE * durations_ms[n] / 1000;
        float freq = NOTES[n];
        const int fade = AUDIO_SAMPLE_RATE / 300;
        for (int i = 0; i < frames; i++) {
            float env = 1.0f;
            if (i < fade)              env = (float)i / fade;
            else if (i > frames - fade) env = (float)(frames - i) / fade;
            // Bell-like decay on the second (longer) note.
            if (n == 1) {
                env *= expf(-2.5f * (float)i / frames);
            }
            float s = sinf(two_pi * freq * i / AUDIO_SAMPLE_RATE) * env;
            int16_t v = (int16_t)(s * 13000);
            buf[2*i]     = v;
            buf[2*i + 1] = v;
        }
        size_t w;
        esp_err_t err = audio_write(buf, frames, &w);
        if (err != ESP_OK) { free(buf); return err; }
    }
    free(buf);
    audio_stop();
    return ESP_OK;
}
