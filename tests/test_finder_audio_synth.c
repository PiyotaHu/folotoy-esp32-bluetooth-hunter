#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "finder_audio_synth.h"

int main(void)
{
    finder_audio_synth_t synth;
    int16_t samples[256];
    size_t total = 0U;
    size_t produced;
    bool nonzero = false;

    finder_audio_synth_begin(&synth, FINDER_CUE_LEVEL, FINDER_LEVEL_NEAR, 123U);
    while (!finder_audio_synth_done(&synth)) {
        produced = finder_audio_synth_render(&synth, samples, 256U);
        assert(produced > 0U);
        for (size_t i = 0U; i < produced; ++i) {
            if (samples[i] != 0) {
                nonzero = true;
            }
        }
        total += produced;
    }
    assert(nonzero);
    assert(total == finder_audio_synth_length(FINDER_CUE_LEVEL));

    finder_audio_synth_begin(&synth, FINDER_CUE_FOUND, FINDER_LEVEL_VERY_CLOSE, 999U);
    total = 0U;
    while (!finder_audio_synth_done(&synth)) {
        produced = finder_audio_synth_render(&synth, samples, 64U);
        total += produced;
    }
    assert(total == finder_audio_synth_length(FINDER_CUE_FOUND));

    puts("test_finder_audio_synth: PASS");
    return 0;
}
