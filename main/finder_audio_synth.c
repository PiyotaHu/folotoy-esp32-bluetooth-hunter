#include "finder_audio_synth.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define ATTACK_SAMPLES 48U
#define RELEASE_SAMPLES 120U
#define BASE_DURATION 1800U

static int16_t saturate_i16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static uint32_t phase_step(uint32_t frequency_hz)
{
    return (frequency_hz * 65536U) / FINDER_AUDIO_RATE;
}

static int32_t envelope(uint32_t position, uint32_t length)
{
    if (position >= length) {
        return 0;
    }
    if (position < ATTACK_SAMPLES) {
        return (int32_t)((position * 32767U) / ATTACK_SAMPLES);
    }
    if (length - position <= RELEASE_SAMPLES) {
        return (int32_t)(((length - position) * 32767U) / RELEASE_SAMPLES);
    }
    return 32767;
}

static int32_t triangle(uint32_t phase)
{
    uint32_t position = phase & 0xffffU;
    int32_t value = position < 32768U ? (int32_t)position : (int32_t)(65535U - position);

    return value * 2 - 32767;
}

static int32_t square(uint32_t phase)
{
    return (phase & 0x8000U) != 0U ? 32767 : -32767;
}

static uint32_t next_random(uint32_t *state)
{
    uint32_t value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static uint32_t frequency_for(const finder_audio_synth_t *synth, uint32_t local)
{
    static const uint16_t found_notes[] = {523U, 659U, 784U, 1047U};
    uint32_t note;

    switch (synth->cue) {
    case FINDER_CUE_LOCK:
        return local < 600U ? 784U : 1175U;
    case FINDER_CUE_CLOSER:
        return 660U + (local * 540U) / 1400U;
    case FINDER_CUE_AWAY:
        return 1040U - (local * 420U) / 1400U;
    case FINDER_CUE_FOUND:
        note = local / 600U;
        return found_notes[note < 4U ? note : 3U];
    case FINDER_CUE_SETTING:
        return 880U;
    case FINDER_CUE_LEVEL:
    default:
        return 320U + (uint32_t)synth->level * 170U;
    }
}

static bool gate_open(const finder_audio_synth_t *synth, uint32_t local)
{
    uint32_t period;
    uint32_t active;

    switch (synth->cue) {
    case FINDER_CUE_LEVEL:
        if (synth->level == FINDER_LEVEL_OUT) {
            period = 900U;
            active = 160U;
            return local % period < active;
        }
        return true;
    case FINDER_CUE_FOUND:
        return (local % 600U) < 420U;
    case FINDER_CUE_LOCK:
    case FINDER_CUE_SETTING:
        return true;
    case FINDER_CUE_CLOSER:
    case FINDER_CUE_AWAY:
    default:
        return (local % 350U) < 210U;
    }
}

size_t finder_audio_synth_length(finder_cue_t cue)
{
    switch (cue) {
    case FINDER_CUE_LOCK:
        return 1400U;
    case FINDER_CUE_CLOSER:
    case FINDER_CUE_AWAY:
        return 1500U;
    case FINDER_CUE_FOUND:
        return 2500U;
    case FINDER_CUE_SETTING:
        return 900U;
    case FINDER_CUE_LEVEL:
    default:
        return BASE_DURATION;
    }
}

void finder_audio_synth_begin(finder_audio_synth_t *synth,
                              finder_cue_t cue,
                              finder_level_t level,
                              uint32_t seed)
{
    if (synth == NULL) {
        return;
    }
    memset(synth, 0, sizeof(*synth));
    synth->cue = cue;
    synth->level = level;
    synth->total_samples = finder_audio_synth_length(cue);
    synth->rng = seed == 0U ? 0x9e3779b9U : seed;
}

size_t finder_audio_synth_render(finder_audio_synth_t *synth,
                                 int16_t *samples,
                                 size_t capacity)
{
    size_t produced = 0U;

    if (synth == NULL || samples == NULL) {
        return 0U;
    }
    while (produced < capacity && synth->sample_index < synth->total_samples) {
        uint32_t local = synth->sample_index;
        uint32_t frequency = frequency_for(synth, local);
        int32_t gain = envelope(local, synth->total_samples);
        int32_t value = 0;

        synth->phase += phase_step(frequency);
        if (gate_open(synth, local)) {
            uint32_t noise = next_random(&synth->rng) % 24U;
            value = ((triangle(synth->phase) * 3 + square(synth->phase)) / 4) *
                    gain / 32767;
            value += ((int32_t)noise - 12) * (int32_t)gain / 32767;
        }
        samples[produced++] = saturate_i16(value * 9000 / 100);
        ++synth->sample_index;
    }
    return produced;
}

bool finder_audio_synth_done(const finder_audio_synth_t *synth)
{
    return synth == NULL || synth->sample_index >= synth->total_samples;
}
