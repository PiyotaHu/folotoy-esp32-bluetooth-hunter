#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_types.h"

/* Short median window: reject a single bad advertisement without lagging
 * several seconds behind a real move away from the device. */
#define SIGNAL_WINDOW 3U
#define SIGNAL_HISTORY 10U

typedef struct {
    int8_t thresholds[FINDER_LEVEL_COUNT - 1];
    uint8_t hysteresis_db;
    uint8_t ema_shift;
    uint32_t unseen_timeout_ms;
    uint32_t trend_window_ms;
} signal_config_t;

typedef struct {
    signal_config_t config;
    int8_t samples[SIGNAL_WINDOW];
    uint8_t sample_count;
    uint8_t sample_index;
    int32_t filtered_q8;
    bool has_filter;
    int8_t best_rssi;
    uint32_t last_seen_ms;
    finder_level_t level;
    struct {
        int32_t rssi_q8;
        uint32_t timestamp_ms;
    } history[SIGNAL_HISTORY];
    uint8_t history_index;
    uint8_t history_count;
    bool has_baseline;
} signal_filter_t;

void signal_config_from_profile(signal_config_t *config, finder_profile_t profile);
void signal_filter_init(signal_filter_t *filter, const signal_config_t *config);
finder_level_t signal_filter_add(signal_filter_t *filter, int8_t rssi, uint32_t now_ms);
finder_level_t signal_filter_tick(signal_filter_t *filter, uint32_t now_ms);
/* Read-only level for rendering. Unlike signal_filter_tick() this never
 * resets samples, so drawing the screen cannot itself lose the signal. */
finder_level_t signal_filter_peek_level(const signal_filter_t *filter,
                                       uint32_t now_ms);
int8_t signal_filter_value(const signal_filter_t *filter);
bool signal_filter_fresh(const signal_filter_t *filter, uint32_t now_ms);
finder_trend_t signal_filter_trend(const signal_filter_t *filter, uint32_t now_ms, int8_t *delta_db);
uint32_t signal_filter_age_ms(const signal_filter_t *filter, uint32_t now_ms);
