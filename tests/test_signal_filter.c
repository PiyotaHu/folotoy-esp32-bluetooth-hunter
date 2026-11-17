#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "signal_filter.h"

static void feed(signal_filter_t *filter, int8_t value, uint32_t start)
{
    for (uint8_t i = 0U; i < 40U; ++i) {
        (void)signal_filter_add(filter, value, start + i * 100U);
    }
}

int main(void)
{
    signal_config_t config;
    signal_filter_t filter;
    int8_t delta = 0;

    signal_config_from_profile(&config, FINDER_PROFILE_NORMAL);
    signal_filter_init(&filter, &config);
    assert(filter.level == FINDER_LEVEL_OUT);
    assert(signal_filter_value(&filter) == INT8_MIN);

    feed(&filter, -90, 1000U);
    assert(filter.level == FINDER_LEVEL_OUT);
    assert(signal_filter_value(&filter) <= -87);

    feed(&filter, -79, 5000U);
    assert(filter.level == FINDER_LEVEL_FAR);

    feed(&filter, -66, 9000U);
    assert(filter.level == FINDER_LEVEL_NEAR);

    feed(&filter, -45, 13000U);
    assert(filter.level == FINDER_LEVEL_VERY_CLOSE);
    assert(signal_filter_fresh(&filter, 17000U));

    (void)signal_filter_add(&filter, -46, 17000U);
    assert(signal_filter_trend(&filter, 17000U, &delta) == FINDER_TREND_STABLE);

    for (int8_t rssi = -44; rssi <= -30; ++rssi) {
        (void)signal_filter_add(&filter, rssi,
                                17100U + (uint32_t)(rssi + 44) * 100U);
    }
    assert(signal_filter_trend(&filter, 18600U, &delta) == FINDER_TREND_CLOSER);
    assert(delta >= 4);

    for (int8_t rssi = -31; rssi >= -70; --rssi) {
        (void)signal_filter_add(&filter, rssi,
                                18700U + (uint32_t)(-31 - rssi) * 100U);
    }
    assert(signal_filter_trend(&filter, 22700U, &delta) == FINDER_TREND_AWAY);
    assert(delta <= -4);

    assert(signal_filter_tick(&filter, 30000U) == FINDER_LEVEL_OUT);
    assert(!signal_filter_fresh(&filter, 30000U));
    assert(signal_filter_value(&filter) == INT8_MIN);

    puts("test_signal_filter: PASS");
    return 0;
}
