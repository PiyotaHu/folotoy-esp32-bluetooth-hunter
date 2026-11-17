#include "signal_filter.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define Q8_ONE 256

/* Real consumer devices advertise far slower than the 100 ms scan interval:
 * idle earbuds, phones and trackers commonly sit at 1-3 s, occasionally more.
 * The old 1.2-2 s "unseen" window reset the filter between ordinary packets,
 * which is why tracking kept falling back to "no signal". Keep these generous
 * enough to ride out real broadcast gaps; the level still decays on tick. */
static const signal_config_t PROFILE_FAST = {
    .thresholds = {-83, -72, -61, -49},
    .hysteresis_db = 3,
    .ema_shift = 2,           // alpha = 1/4: responsive, still damped
    .unseen_timeout_ms = 4000,
    .trend_window_ms = 1400,
};

static const signal_config_t PROFILE_NORMAL = {
    .thresholds = {-83, -72, -61, -49},
    .hysteresis_db = 4,
    .ema_shift = 3,           // alpha = 1/8
    .unseen_timeout_ms = 5000,
    .trend_window_ms = 1800,
};

static const signal_config_t PROFILE_STABLE = {
    .thresholds = {-83, -72, -61, -49},
    .hysteresis_db = 5,
    .ema_shift = 4,           // alpha = 1/16
    .unseen_timeout_ms = 6500,
    .trend_window_ms = 2400,
};

static bool config_valid(const signal_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    for (size_t i = 1; i < FINDER_LEVEL_COUNT - 1U; ++i) {
        if (config->thresholds[i - 1U] >= config->thresholds[i]) {
            return false;
        }
    }
    return config->ema_shift >= 1U && config->ema_shift <= 6U &&
           config->unseen_timeout_ms > 0U && config->trend_window_ms > 0U;
}

void signal_config_from_profile(signal_config_t *config, finder_profile_t profile)
{
    const signal_config_t *source = &PROFILE_NORMAL;

    if (config == NULL) {
        return;
    }
    if (profile == FINDER_PROFILE_FAST) {
        source = &PROFILE_FAST;
    } else if (profile == FINDER_PROFILE_STABLE) {
        source = &PROFILE_STABLE;
    }
    *config = *source;
}

static int32_t q8_round(int32_t q8)
{
    int32_t rounded = q8;

    if (rounded >= 0) {
        rounded += Q8_ONE / 2;
    } else {
        rounded -= Q8_ONE / 2;
    }
    return rounded / Q8_ONE;
}

static int8_t median_of_samples(const signal_filter_t *filter)
{
    int8_t sorted[SIGNAL_WINDOW];
    uint8_t count = filter->sample_count;

    if (count == 0U) {
        return INT8_MIN;
    }
    memcpy(sorted, filter->samples, (size_t)count * sizeof(sorted[0]));
    for (uint8_t i = 1U; i < count; ++i) {
        int8_t value = sorted[i];
        uint8_t position = i;

        while (position > 0U && sorted[position - 1U] > value) {
            sorted[position] = sorted[position - 1U];
            --position;
        }
        sorted[position] = value;
    }
    return sorted[count / 2U];
}

static finder_level_t classify_raw(const signal_filter_t *filter, int8_t rssi)
{
    for (uint8_t i = 0; i < FINDER_LEVEL_COUNT - 1U; ++i) {
        if (rssi < filter->config.thresholds[i]) {
            return (finder_level_t)i;
        }
    }
    return (finder_level_t)(FINDER_LEVEL_COUNT - 1U);
}

static finder_level_t apply_hysteresis(const signal_filter_t *filter, int8_t rssi)
{
    finder_level_t level = filter->level;
    int16_t threshold;
    int16_t value = rssi;

    while (level < FINDER_LEVEL_VERY_CLOSE) {
        threshold = (int16_t)filter->config.thresholds[level] +
                    (int16_t)filter->config.hysteresis_db;
        if (value < threshold) {
            break;
        }
        level = (finder_level_t)(level + 1);
    }
    while (level > FINDER_LEVEL_OUT) {
        threshold = (int16_t)filter->config.thresholds[level - 1U] -
                    (int16_t)filter->config.hysteresis_db;
        if (value >= threshold) {
            break;
        }
        level = (finder_level_t)(level - 1);
    }
    return level;
}

static void reset_samples(signal_filter_t *filter)
{
    filter->sample_count = 0U;
    filter->sample_index = 0U;
    filter->has_filter = false;
    filter->history_count = 0U;
    filter->history_index = 0U;
    filter->has_baseline = false;
    filter->best_rssi = INT8_MIN;
    filter->level = FINDER_LEVEL_OUT;
}

void signal_filter_init(signal_filter_t *filter, const signal_config_t *config)
{
    signal_config_t defaults;

    if (filter == NULL) {
        return;
    }
    if (!config_valid(config)) {
        signal_config_from_profile(&defaults, FINDER_PROFILE_NORMAL);
        config = &defaults;
    }
    memset(filter, 0, sizeof(*filter));
    filter->config = *config;
    filter->best_rssi = INT8_MIN;
    filter->level = FINDER_LEVEL_OUT;
}

finder_level_t signal_filter_add(signal_filter_t *filter, int8_t rssi, uint32_t now_ms)
{
    int8_t median;
    int32_t target_q8;
    int32_t delta;

    if (filter == NULL) {
        return FINDER_LEVEL_OUT;
    }
    if (filter->has_filter &&
        (uint32_t)(now_ms - filter->last_seen_ms) > filter->config.unseen_timeout_ms) {
        reset_samples(filter);
    }

    filter->samples[filter->sample_index] = rssi;
    filter->sample_index = (uint8_t)((filter->sample_index + 1U) % SIGNAL_WINDOW);
    if (filter->sample_count < SIGNAL_WINDOW) {
        ++filter->sample_count;
    }
    filter->last_seen_ms = now_ms;
    median = median_of_samples(filter);

    if (!filter->has_filter) {
        filter->filtered_q8 = (int32_t)median * Q8_ONE;
        filter->has_filter = true;
        filter->level = classify_raw(filter, median);
    } else {
        target_q8 = (int32_t)median * Q8_ONE;
        delta = target_q8 - filter->filtered_q8;
        filter->filtered_q8 += delta >> filter->config.ema_shift;
        filter->level = apply_hysteresis(filter, (int8_t)q8_round(filter->filtered_q8));
    }

    if (filter->best_rssi == INT8_MIN || median > filter->best_rssi) {
        filter->best_rssi = median;
        filter->has_baseline = true;
    }

    filter->history[filter->history_index].rssi_q8 = filter->filtered_q8;
    filter->history[filter->history_index].timestamp_ms = now_ms;
    filter->history_index = (uint8_t)((filter->history_index + 1U) % SIGNAL_HISTORY);
    if (filter->history_count < SIGNAL_HISTORY) {
        ++filter->history_count;
    }
    return filter->level;
}

finder_level_t signal_filter_tick(signal_filter_t *filter, uint32_t now_ms)
{
    if (filter == NULL) {
        return FINDER_LEVEL_OUT;
    }
    if (!signal_filter_fresh(filter, now_ms)) {
        reset_samples(filter);
    }
    return filter->level;
}

finder_level_t signal_filter_peek_level(const signal_filter_t *filter,
                                       uint32_t now_ms)
{
    if (filter == NULL || !signal_filter_fresh(filter, now_ms)) {
        return FINDER_LEVEL_OUT;
    }
    return filter->level;
}

int8_t signal_filter_value(const signal_filter_t *filter)
{
    if (filter == NULL || !filter->has_filter) {
        return INT8_MIN;
    }
    return (int8_t)q8_round(filter->filtered_q8);
}

bool signal_filter_fresh(const signal_filter_t *filter, uint32_t now_ms)
{
    return filter != NULL && filter->has_filter &&
           (uint32_t)(now_ms - filter->last_seen_ms) <= filter->config.unseen_timeout_ms;
}

finder_trend_t signal_filter_trend(const signal_filter_t *filter, uint32_t now_ms,
                                   int8_t *delta_db)
{
    uint32_t oldest_ms;
    int32_t delta_q8;
    int8_t delta;
    uint8_t index;
    uint8_t age;

    if (delta_db != NULL) {
        *delta_db = 0;
    }
    if (filter == NULL || !signal_filter_fresh(filter, now_ms) ||
        filter->history_count < 3U) {
        return FINDER_TREND_STABLE;
    }
    oldest_ms = now_ms - filter->config.trend_window_ms;
    index = (uint8_t)((filter->history_index + SIGNAL_HISTORY -
                       filter->history_count) % SIGNAL_HISTORY);
    age = (uint8_t)((now_ms - filter->history[index].timestamp_ms) / 100U);
    if ((uint32_t)age * 100U < filter->config.trend_window_ms / 2U) {
        return FINDER_TREND_STABLE;
    }
    (void)oldest_ms;
    delta_q8 = filter->filtered_q8 - filter->history[index].rssi_q8;
    delta = (int8_t)q8_round(delta_q8);
    if (delta_db != NULL) {
        *delta_db = delta;
    }
    if (delta >= 4) {
        return FINDER_TREND_CLOSER;
    }
    if (delta <= -4) {
        return FINDER_TREND_AWAY;
    }
    return FINDER_TREND_STABLE;
}

uint32_t signal_filter_age_ms(const signal_filter_t *filter, uint32_t now_ms)
{
    if (filter == NULL || !filter->has_filter) {
        return UINT32_MAX;
    }
    return (uint32_t)(now_ms - filter->last_seen_ms);
}
