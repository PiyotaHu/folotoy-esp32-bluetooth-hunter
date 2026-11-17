/*
 * Host simulator for BLE Signal Finder.
 * Runs the exact firmware model with synthetic advertisement reports.
 * No ESP-IDF, LVGL, or hardware required.
 */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "finder_model.h"

#define SIM_STEPS 120U

typedef struct {
    uint32_t seed;
} sim_rng_t;

static uint32_t next_rand(sim_rng_t *rng)
{
    rng->seed ^= rng->seed << 13;
    rng->seed ^= rng->seed >> 17;
    rng->seed ^= rng->seed << 5;
    return rng->seed;
}

static int8_t jitter(sim_rng_t *rng, int8_t rssi)
{
    int32_t value = rssi + (int32_t)(next_rand(rng) % 7U) - 3;

    if (value < -100) {
        value = -100;
    } else if (value > -20) {
        value = -20;
    }
    return (int8_t)value;
}

static void emit(finder_model_t *model, const uint8_t addr[6], int8_t rssi,
                 uint32_t now)
{
    ble_scan_report_t report;

    memset(&report, 0, sizeof(report));
    memcpy(report.addr, addr, sizeof(report.addr));
    report.addr_type = 1U;
    report.rssi = rssi;
    report.timestamp_ms = now;
    (void)finder_model_on_report(model, &report, now);
}

static void print_frame(const finder_ui_model_t *view, uint32_t now)
{
    const char *level_names[] = {"OUT  ", "FAR  ", "NEAR ", "CLOSE", "VERY "};

    printf("%6u ms | %-6s | %4d dBm | %-6s | present=%d\n",
           (unsigned)now, level_names[view->level],
           view->target_rssi == INT8_MIN ? 0 : (int)view->target_rssi,
           view->trend == FINDER_TREND_CLOSER ? "CLOSER" :
           view->trend == FINDER_TREND_AWAY ? "AWAY" : "STABLE",
           view->target_present ? 1 : 0);
}

int main(void)
{
    static const uint8_t phone[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    static const uint8_t earbuds[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    sim_rng_t rng = {0xC0FFEEU};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    finder_target_t target;
    uint32_t now = 1000U;
    uint8_t frames = 0U;
    bool saw_far = false;
    bool saw_close = false;
    bool saw_very_close = false;
    bool saw_closer = false;
    bool saw_away = false;
    int8_t base = -86;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, now);

    printf("=== BLE Signal Finder host simulation ===\n");
    printf("step 1: discover phone and earbuds, lock the stronger device\n");
    emit(&model, phone, -76, now);
    emit(&model, earbuds, -58, now + 5U);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, now + 10U);
    target = finder_model_target(&model);
    assert(memcmp(target.addr, earbuds, sizeof(earbuds)) == 0);
    printf("locked target: %02X:%02X:%02X:%02X:%02X:%02X\n",
           target.addr[0], target.addr[1], target.addr[2],
           target.addr[3], target.addr[4], target.addr[5]);

    // Fresh boot with the saved target: this is the normal find-after-reboot path.
    finder_model_t walk_model;
    finder_model_init(&walk_model, &prefs, &target, true, now + 20U);
    model = walk_model;

    printf("step 2: walk towards target (-86 -> -35 dBm)\n");
    for (uint8_t i = 0U; i < SIM_STEPS / 2U; ++i) {
        now += 100U;
        if (i < 20U) {
            base = (int8_t)(-86 + (int)i);
        } else {
            base = (int8_t)(-66 + (int)(i - 20U));
        }
        emit(&model, earbuds, jitter(&rng, base), now);
        emit(&model, phone, jitter(&rng, -78), now + 1U);
        (void)finder_model_tick(&model, now);
        finder_model_view(&model, &view, now);
        if (view.level == FINDER_LEVEL_FAR) saw_far = true;
        if (view.level == FINDER_LEVEL_CLOSE) saw_close = true;
        if (view.level == FINDER_LEVEL_VERY_CLOSE) saw_very_close = true;
        if (view.trend == FINDER_TREND_CLOSER) saw_closer = true;
        if (frames++ % 10U == 0U) {
            print_frame(&view, now);
        }
    }

    printf("step 3: walk away (-35 -> -82 dBm)\n");
    for (uint8_t i = 0U; i < SIM_STEPS / 2U; ++i) {
        now += 100U;
        base = (int8_t)(-36 - (int)i);
        emit(&model, earbuds, jitter(&rng, base), now);
        (void)finder_model_tick(&model, now);
        finder_model_view(&model, &view, now);
        if (view.trend == FINDER_TREND_AWAY) saw_away = true;
        if (frames++ % 10U == 0U) {
            print_frame(&view, now);
        }
    }

    printf("step 4: target disappears, timeout to NO SIGNAL\n");
    now += 6000U;
    (void)finder_model_tick(&model, now);
    finder_model_view(&model, &view, now);
    assert(view.level == FINDER_LEVEL_OUT);
    assert(!view.target_present);
    print_frame(&view, now);

    printf("step 5: settings round-trip\n");
    (void)finder_model_on_button(&model, FINDER_BTN_OK_LONG, now);
    assert(model.state == FINDER_STATE_SETTINGS);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, now + 10U);
    assert(!model.prefs.sound_enabled);
    (void)finder_model_on_button(&model, FINDER_BTN_OK_LONG, now + 20U);
    assert(model.state == FINDER_STATE_TRACK);

    assert(saw_far);
    assert(saw_close);
    assert(saw_very_close);
    assert(saw_closer);
    assert(saw_away);

    printf("simulation: PASS (far/close/very-close, both trends, loss timeout)\n");
    return 0;
}
