#include "app/mp3_player.h"

#include <stdlib.h>
#include <string.h>

#include "drivers/max98357a.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "decoder/esp_audio_dec_default.h"
#include "simple_dec/esp_audio_simple_dec.h"
#include "simple_dec/esp_audio_simple_dec_default.h"

static const char *TAG = "player";

#define MP3_IN_FEED_CHUNK 1536
#define MP3_OUT_PCM_INIT  4096
#define MP3_BAR_COUNT     16
#define MP3_WAVEFORM_SAMPLES 240

#define MP3_PART_MAGIC       0x33504d43u
#define MUSIC_PARTITION_NAME "music_nvs"

typedef struct {
    uint32_t magic;
    uint32_t payload_size;
    uint32_t reserved0;
    uint32_t reserved1;
} mp3_partition_header_t;

typedef struct {
    TaskHandle_t task;
    SemaphoreHandle_t lock;
    bool enabled;
    bool has_track;
    bool decoder_ready;

    uint8_t *track_data;
    size_t track_size;

    esp_audio_simple_dec_handle_t dec;
    uint8_t bars[MP3_BAR_COUNT];
    uint8_t waveform[MP3_WAVEFORM_SAMPLES];
    size_t waveform_idx;
    uint8_t bass_energy;
} mp3_player_ctx_t;

static mp3_player_ctx_t s_player;

static void mp3_player_clear_bars(void)
{
    if (s_player.lock) {
        xSemaphoreTake(s_player.lock, portMAX_DELAY);
        memset(s_player.bars, 0, sizeof(s_player.bars));
        memset(s_player.waveform, 0, sizeof(s_player.waveform));
        s_player.waveform_idx = 0;
        xSemaphoreGive(s_player.lock);
    } else {
        memset(s_player.bars, 0, sizeof(s_player.bars));
        memset(s_player.waveform, 0, sizeof(s_player.waveform));
        s_player.waveform_idx = 0;
    }
}

static void mp3_player_update_spectrum(const int16_t *pcm, size_t frames, uint8_t channels)
{
    if (!pcm || frames == 0 || (channels != 1 && channels != 2)) {
        return;
    }

    uint8_t instant[MP3_BAR_COUNT];
    for (size_t b = 0; b < MP3_BAR_COUNT; b++) {
        size_t start = (b * frames) / MP3_BAR_COUNT;
        size_t end = ((b + 1) * frames) / MP3_BAR_COUNT;
        if (end <= start) {
            instant[b] = 0;
            continue;
        }

        uint64_t acc = 0;
        for (size_t i = start; i < end; i++) {
            int32_t sample;
            if (channels == 2) {
                int32_t l = pcm[2 * i];
                int32_t r = pcm[2 * i + 1];
                sample = (l + r) / 2;
            } else {
                sample = pcm[i];
            }
            if (sample < 0) sample = -sample;
            acc += (uint32_t)sample;
        }

        uint32_t avg = (uint32_t)(acc / (end - start));
        uint32_t level = (avg * 100U) / 28000U;
        if (level > 100U) level = 100U;
        instant[b] = (uint8_t)level;
    }

    xSemaphoreTake(s_player.lock, portMAX_DELAY);
    for (size_t i = 0; i < MP3_BAR_COUNT; i++) {
        uint16_t smooth = (uint16_t)s_player.bars[i] * 7U + (uint16_t)instant[i] * 3U;
        s_player.bars[i] = (uint8_t)(smooth / 10U);
    }
    uint16_t bass = ((uint16_t)instant[0] + instant[1] + instant[2]) / 3U;
    s_player.bass_energy = (uint8_t)(bass > 100U ? 100U : bass);
    
    // Update waveform buffer with downsampled PCM data
    size_t step = (frames > MP3_WAVEFORM_SAMPLES) ? (frames / MP3_WAVEFORM_SAMPLES) : 1;
    for (size_t i = 0; i < frames && s_player.waveform_idx < MP3_WAVEFORM_SAMPLES; i += step) {
        int32_t sample;
        if (channels == 2) {
            int32_t l = pcm[2 * i];
            int32_t r = pcm[2 * i + 1];
            sample = (l + r) / 2;
        } else {
            sample = pcm[i];
        }
        // Convert to 0-255 range: map -32768..32767 to 0..255
        uint32_t val = (uint32_t)(sample + 32768) / 256;
        if (val > 255) val = 255;
        s_player.waveform[s_player.waveform_idx++] = (uint8_t)val;
    }
    if (s_player.waveform_idx >= MP3_WAVEFORM_SAMPLES) {
        s_player.waveform_idx = 0;  // Wrap around
    }
    xSemaphoreGive(s_player.lock);
}

static esp_err_t mp3_player_load_track_from_partition(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                           ESP_PARTITION_SUBTYPE_ANY,
                                                           MUSIC_PARTITION_NAME);
    ESP_RETURN_ON_FALSE(part != NULL, ESP_ERR_NOT_FOUND, TAG, "partition %s not found", MUSIC_PARTITION_NAME);

    mp3_partition_header_t header = { 0 };
    ESP_RETURN_ON_ERROR(esp_partition_read(part, 0, &header, sizeof(header)), TAG, "read music header");
    ESP_RETURN_ON_FALSE(header.magic == MP3_PART_MAGIC, ESP_ERR_NOT_FOUND, TAG, "music image missing");
    ESP_RETURN_ON_FALSE(header.payload_size > 0, ESP_ERR_INVALID_SIZE, TAG, "music image empty");
    ESP_RETURN_ON_FALSE(header.payload_size <= (part->size - sizeof(header)), ESP_ERR_INVALID_SIZE,
                        TAG, "music image too large for partition");

    size_t blob_size = header.payload_size;
    uint8_t *track = heap_caps_malloc(blob_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!track) {
        track = heap_caps_malloc(blob_size, MALLOC_CAP_8BIT);
    }
    if (!track) {
        ESP_LOGE(TAG, "no memory for track blob: %u", (unsigned)blob_size);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_partition_read(part, sizeof(header), track, blob_size);
    if (err != ESP_OK) {
        free(track);
        ESP_RETURN_ON_ERROR(err, TAG, "read track payload");
    }

    s_player.track_data = track;
    s_player.track_size = blob_size;
    s_player.has_track = true;

    ESP_LOGI(TAG, "loaded MP3 from partition %s size=%u bytes", MUSIC_PARTITION_NAME, (unsigned)blob_size);
    return ESP_OK;
}

static esp_err_t mp3_player_decode_and_play_once(void)
{
    if (!s_player.enabled || !s_player.has_track || !s_player.dec) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_audio_simple_dec_info_t info = { 0 };
    bool info_ready = false;
    size_t offset = 0;

    uint32_t out_size = MP3_OUT_PCM_INIT;
    uint8_t *out_buf = heap_caps_malloc(out_size, MALLOC_CAP_8BIT);
    if (!out_buf) {
        return ESP_ERR_NO_MEM;
    }

    while (s_player.enabled) {
        if (offset >= s_player.track_size) {
            esp_audio_simple_dec_reset(s_player.dec);
            offset = 0;
        }

        size_t remain = s_player.track_size - offset;
        size_t feed = remain > MP3_IN_FEED_CHUNK ? MP3_IN_FEED_CHUNK : remain;

        esp_audio_simple_dec_raw_t raw = {
            .buffer = s_player.track_data + offset,
            .len = (uint32_t)feed,
            .eos = (offset + feed) >= s_player.track_size,
            .consumed = 0,
            .frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE,
        };

        while (s_player.enabled && raw.len > 0) {
            esp_audio_simple_dec_out_t frame = {
                .buffer = out_buf,
                .len = out_size,
                .needed_size = 0,
                .decoded_size = 0,
            };

            esp_audio_err_t derr = esp_audio_simple_dec_process(s_player.dec, &raw, &frame);
            if (derr == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH && frame.needed_size > out_size) {
                uint8_t *new_buf = realloc(out_buf, frame.needed_size);
                if (!new_buf) {
                    free(out_buf);
                    return ESP_ERR_NO_MEM;
                }
                out_buf = new_buf;
                out_size = frame.needed_size;
                continue;
            }
            if (derr != ESP_AUDIO_ERR_OK) {
                ESP_LOGW(TAG, "decode err=%d, reset stream", (int)derr);
                esp_audio_simple_dec_reset(s_player.dec);
                break;
            }

            offset += raw.consumed;
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;

            if (!info_ready && frame.decoded_size > 0) {
                if (esp_audio_simple_dec_get_info(s_player.dec, &info) == ESP_AUDIO_ERR_OK) {
                    info_ready = true;
                    ESP_LOGI(TAG, "mp3 info: %u Hz, %u ch, %u bits",
                             (unsigned)info.sample_rate,
                             (unsigned)info.channel,
                             (unsigned)info.bits_per_sample);
                }
            }

            if (frame.decoded_size == 0) {
                continue;
            }

            if (!info_ready || info.bits_per_sample != 16) {
                continue;
            }

            size_t bytes_per_frame = (size_t)info.channel * sizeof(int16_t);
            if (bytes_per_frame == 0) {
                continue;
            }
            size_t frames = frame.decoded_size / bytes_per_frame;
            const int16_t *pcm = (const int16_t *)frame.buffer;

            if (info.channel == 1) {
                int16_t *stereo = heap_caps_malloc(frames * 2 * sizeof(int16_t), MALLOC_CAP_8BIT);
                if (!stereo) {
                    continue;
                }
                for (size_t i = 0; i < frames; i++) {
                    stereo[2 * i] = pcm[i];
                    stereo[2 * i + 1] = pcm[i];
                }
                size_t written = 0;
                audio_write(stereo, frames, &written);
                mp3_player_update_spectrum(stereo, frames, 2);
                free(stereo);
                vTaskDelay(1);
            } else {
                size_t written = 0;
                audio_write(pcm, frames, &written);
                mp3_player_update_spectrum(pcm, frames, info.channel);
                vTaskDelay(1);
            }
        }
    }

    free(out_buf);
    return ESP_OK;
}

static void mp3_player_task(void *arg)
{
    while (1) {
        if (!s_player.enabled || !s_player.has_track) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
            continue;
        }

        (void)mp3_player_decode_and_play_once();
        if (!s_player.enabled) {
            mp3_player_clear_bars();
            audio_stream_stop();
        }
    }
}

esp_err_t mp3_player_init(void)
{
    if (s_player.task) {
        return ESP_OK;
    }

    memset(&s_player, 0, sizeof(s_player));
    s_player.lock = xSemaphoreCreateMutex();
    if (!s_player.lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_audio_err_t aerr = esp_audio_dec_register_default();
    if (aerr != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "register decoder core failed: %d", (int)aerr);
        return ESP_FAIL;
    }

    aerr = esp_audio_simple_dec_register_default();
    if (aerr != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "register decoder default failed: %d", (int)aerr);
        return ESP_FAIL;
    }

    esp_audio_simple_dec_cfg_t cfg = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,
        .cfg_size = 0,
        .use_frame_dec = false,
    };
    aerr = esp_audio_simple_dec_open(&cfg, &s_player.dec);
    if (aerr != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "open mp3 simple decoder failed: %d", (int)aerr);
        return ESP_FAIL;
    }
    s_player.decoder_ready = true;

    esp_err_t err = mp3_player_load_track_from_partition();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "track not available, player tab shows IDLE");
    }

    BaseType_t ok = xTaskCreatePinnedToCore(mp3_player_task, "mp3_player", 8192,
                                            NULL, 5, &s_player.task, 1);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    mp3_player_clear_bars();
    return ESP_OK;
}

esp_err_t mp3_player_set_enabled(bool enable)
{
    if (!s_player.task) {
        return ESP_ERR_INVALID_STATE;
    }
    s_player.enabled = enable;
    xTaskNotifyGive(s_player.task);
    if (!enable) {
        mp3_player_clear_bars();
    }
    return ESP_OK;
}

bool mp3_player_is_enabled(void)
{
    return s_player.enabled;
}

bool mp3_player_has_track(void)
{
    return s_player.has_track;
}

void mp3_player_get_spectrum(uint8_t *bars, size_t n_bars)
{
    if (!bars || n_bars == 0) {
        return;
    }

    if (!s_player.lock) {
        memset(bars, 0, n_bars);
        return;
    }

    xSemaphoreTake(s_player.lock, portMAX_DELAY);
    for (size_t i = 0; i < n_bars; i++) {
        bars[i] = s_player.bars[i % MP3_BAR_COUNT];
    }
    xSemaphoreGive(s_player.lock);
}

void mp3_player_get_waveform(uint8_t *wave, size_t n_samples)
{
    if (!wave || n_samples == 0) {
        return;
    }

    if (!s_player.lock) {
        memset(wave, 128, n_samples);
        return;
    }

    xSemaphoreTake(s_player.lock, portMAX_DELAY);
    size_t count = n_samples > MP3_WAVEFORM_SAMPLES ? MP3_WAVEFORM_SAMPLES : n_samples;
    memcpy(wave, s_player.waveform, count);
    if (count < n_samples) {
        memset(wave + count, 128, n_samples - count);  // Fill with midpoint (silence)
    }
    xSemaphoreGive(s_player.lock);
}

uint8_t mp3_player_get_bass_energy(void)
{
    if (!s_player.lock) return 0;
    xSemaphoreTake(s_player.lock, portMAX_DELAY);
    uint8_t e = s_player.bass_energy;
    xSemaphoreGive(s_player.lock);
    return e;
}

void mp3_player_get_band_energy(uint8_t *bass, uint8_t *mid, uint8_t *treble)
{
    if (!s_player.lock) {
        if (bass) *bass = 0;
        if (mid) *mid = 0;
        if (treble) *treble = 0;
        return;
    }
    xSemaphoreTake(s_player.lock, portMAX_DELAY);
    if (bass) {
        uint16_t sum = 0;
        for (int i = 0; i < 4; i++) sum += s_player.bars[i];
        *bass = (uint8_t)(sum / 4);
    }
    if (mid) {
        uint16_t sum = 0;
        for (int i = 4; i < 11; i++) sum += s_player.bars[i];
        *mid = (uint8_t)(sum / 7);
    }
    if (treble) {
        uint16_t sum = 0;
        for (int i = 11; i < 16; i++) sum += s_player.bars[i];
        *treble = (uint8_t)(sum / 5);
    }
    xSemaphoreGive(s_player.lock);
}
