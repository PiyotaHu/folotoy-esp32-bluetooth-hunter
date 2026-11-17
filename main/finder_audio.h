#pragma once

#include "esp_err.h"
#include "finder_types.h"
#include "finder_audio_synth.h"

esp_err_t finder_audio_init(void);
esp_err_t finder_audio_play(finder_cue_t cue, finder_level_t level);
esp_err_t finder_audio_set_volume(uint8_t volume);
