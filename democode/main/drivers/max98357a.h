#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t audio_init(void);
esp_err_t audio_self_test(void);
esp_err_t audio_play_chime(void);   // short startup chime, blocking
esp_err_t audio_play_click(void);   // short UI click, blocking
esp_err_t audio_play_tick(void);    // very short encoder detent tick
esp_err_t audio_play_scan_start(void); // rising two-tone "boop", blocking
esp_err_t audio_play_scan_done(void);  // descending bell "ding-dong", blocking
esp_err_t audio_write(const int16_t *samples, size_t n_frames, size_t *written);
esp_err_t audio_stream_stop(void);  // flush and stop I2S stream
esp_err_t audio_set_volume(uint8_t pct);   // software gain 0..100
uint8_t   audio_get_volume(void);
