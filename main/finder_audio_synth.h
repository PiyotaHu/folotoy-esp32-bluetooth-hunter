#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "finder_types.h"

#define FINDER_AUDIO_RATE 16000U

typedef enum {
    FINDER_CUE_LEVEL = 0,
    FINDER_CUE_LOCK,
    FINDER_CUE_CLOSER,
    FINDER_CUE_AWAY,
    FINDER_CUE_FOUND,
    FINDER_CUE_SETTING,
} finder_cue_t;

typedef struct {
    finder_cue_t cue;
    finder_level_t level;
    uint32_t sample_index;
    uint32_t total_samples;
    uint32_t phase;
    uint32_t rng;
} finder_audio_synth_t;

void finder_audio_synth_begin(finder_audio_synth_t *synth,
                              finder_cue_t cue,
                              finder_level_t level,
                              uint32_t seed);
size_t finder_audio_synth_render(finder_audio_synth_t *synth,
                                 int16_t *samples,
                                 size_t capacity);
bool finder_audio_synth_done(const finder_audio_synth_t *synth);
size_t finder_audio_synth_length(finder_cue_t cue);
