#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t mp3_player_init(void);
esp_err_t mp3_player_set_enabled(bool enable);
bool mp3_player_is_enabled(void);
bool mp3_player_has_track(void);
void mp3_player_get_spectrum(uint8_t *bars, size_t n_bars);
void mp3_player_get_waveform(uint8_t *wave, size_t n_samples);
uint8_t mp3_player_get_bass_energy(void);
void mp3_player_get_band_energy(uint8_t *bass, uint8_t *mid, uint8_t *treble);
