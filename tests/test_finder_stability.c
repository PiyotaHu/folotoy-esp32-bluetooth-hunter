#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "finder_model.h"

static ble_scan_report_t make_report(const uint8_t addr[6], int8_t rssi)
{
    ble_scan_report_t report;

    memset(&report, 0, sizeof(report));
    memcpy(report.addr, addr, sizeof(report.addr));
    report.addr_type = 1U;
    report.rssi = rssi;
    return report;
}

static void emit(finder_model_t *model, const uint8_t addr[6], int8_t rssi,
                uint32_t now)
{
    ble_scan_report_t report = make_report(addr, rssi);

    report.timestamp_ms = now;
    (void)finder_model_on_report(model, &report, now);
}

static void emit_named(finder_model_t *model, const uint8_t addr[6],
                       const char *name, int8_t rssi, uint32_t now)
{
    ble_scan_report_t report = make_report(addr, rssi);

    (void)snprintf(report.name, sizeof(report.name), "%s", name);
    (void)snprintf(report.label, sizeof(report.label), "%s", name);
    report.has_name = true;
    report.timestamp_ms = now;
    (void)finder_model_on_report(model, &report, now);
}

static void check_list_stays_still(void)
{
    static const uint8_t a[6] = {0xAA, 1, 2, 3, 4, 5};
    static const uint8_t b[6] = {0xBB, 1, 2, 3, 4, 5};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    uint8_t first_a = 0U, first_b = 0U;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, 1000U);
    emit(&model, a, -70, 1000U);
    emit(&model, b, -70, 1010U);
    finder_model_view(&model, &view, 1020U);
    assert(view.view_count == 2U);
    for (uint8_t i = 0U; i < view.view_count; ++i) {
        if (memcmp(view.devices[i].addr, a, 6) == 0) first_a = i;
        if (memcmp(view.devices[i].addr, b, 6) == 0) first_b = i;
    }
    /* Aggressive RSSI jitter must not reshuffle the rows. The previous
     * implementation re-sorted on every live RSSI sample, so rows bounced. */
    for (uint32_t t = 1100U; t < 9000U; t += 100U) {
        emit(&model, a, (int8_t)(-60 - (int)(t % 7U) * 3), t);
        emit(&model, b, (int8_t)(-90 + (int)(t % 9U) * 3), t + 1U);
        finder_model_view(&model, &view, t + 2U);
        assert(view.view_count == 2U);
        for (uint8_t i = 0U; i < view.view_count; ++i) {
            if (memcmp(view.devices[i].addr, a, 6) == 0) assert(i == first_a);
            if (memcmp(view.devices[i].addr, b, 6) == 0) assert(i == first_b);
        }
    }
}

static void check_cursor_follows_device(void)
{
    static const uint8_t a[6] = {0xAA, 1, 2, 3, 4, 5};
    static const uint8_t b[6] = {0xBB, 1, 2, 3, 4, 5};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    finder_target_t target;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, 1000U);
    emit(&model, a, -60, 1000U);
    emit(&model, b, -80, 1010U);
    /* Select the second row (b), then make a much stronger new device appear
     * so rows shift. The cursor must stay on b. */
    (void)finder_model_on_button(&model, FINDER_BTN_DOWN, 1020U);
    {
        static const uint8_t c[6] = {0xCC, 1, 2, 3, 4, 5};
        emit(&model, c, -30, 1030U);
    }
    finder_model_view(&model, &view, 1040U);
    assert(view.view_count == 3U);
    assert(memcmp(view.devices[view.cursor].addr, b, 6) == 0);
    /* OK must lock the highlighted device, not whatever happens to be row 1. */
    (void)finder_model_on_button(&model, FINDER_BTN_OK, 1050U);
    target = finder_model_target(&model);
    assert(memcmp(target.addr, b, 6) == 0);
}

static void check_tracking_survives_gaps_and_crowding(void)
{
    static const uint8_t target[6] = {0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    finder_target_t locked;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, 1000U);
    emit_named(&model, target, "AirPods Pro", -55, 1000U);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, 1010U);
    locked = finder_model_target(&model);
    assert(memcmp(locked.addr, target, 6) == 0);

    /* Immediately after locking the tracking screen must show the signal
     * already measured on the scan screen instead of "no signal". */
    finder_model_view(&model, &view, 1020U);
    assert(view.target_present);
    assert(view.target_rssi != INT8_MIN);
    assert(view.level != FINDER_LEVEL_OUT);

    /* Idle consumer devices often advertise every 2-3 s. That must not be
     * treated as losing the target. */
    for (uint32_t t = 3000U; t < 12000U; t += 2500U) {
        emit_named(&model, target, "AirPods Pro", -52, t);
        (void)finder_model_tick(&model, t + 100U);
        finder_model_view(&model, &view, t + 1200U);
        assert(view.target_present);
        assert(view.level != FINDER_LEVEL_OUT);
    }

    /* Flood the 12-slot list with stronger devices. The target row must not
     * be evicted and tracking must keep its own filter. */
    for (uint8_t i = 0U; i < 30U; ++i) {
        uint8_t other[6] = {0xE0, 0, 0, 0, 0, i};
        emit(&model, other, -35, 13000U + i * 10U);
    }
    emit_named(&model, target, "AirPods Pro", -50, 14000U);
    finder_model_view(&model, &view, 14010U);
    assert(view.target_present);
    assert(view.level != FINDER_LEVEL_OUT);
    {
        bool target_in_list = false;
        for (uint8_t i = 0U; i < view.view_count; ++i) {
            if (memcmp(view.devices[i].addr, target, 6) == 0) {
                target_in_list = true;
            }
        }
        assert(target_in_list);
    }
}

static void check_private_address_rotation(void)
{
    static const uint8_t original[6] = {0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F};
    static const uint8_t rotated[6] = {0x71, 0x72, 0x73, 0x74, 0x75, 0x76};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    finder_target_t locked;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, 1000U);
    emit_named(&model, original, "iPhone", -60, 1000U);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, 1010U);
    /* Phones rotate their resolvable private address. After the old address
     * goes quiet, a same-name report must re-bind instead of losing tracking. */
    emit_named(&model, rotated, "iPhone", -58, 5000U);
    locked = finder_model_target(&model);
    assert(memcmp(locked.addr, rotated, 6) == 0);
    emit_named(&model, rotated, "iPhone", -57, 5100U);
    (void)finder_model_tick(&model, 5110U);
    finder_model_view(&model, &view, 5120U);
    assert(view.target_present);
    assert(view.level != FINDER_LEVEL_OUT);
}

static void check_grace_period_after_lock(void)
{
    static const uint8_t a[6] = {0xAA, 1, 2, 3, 4, 5};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, 1000U);
    emit(&model, a, -70, 1000U);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, 1010U);
    /* A restored target from storage has no live samples yet. The UI must show
     * an acquiring state for the grace window rather than "no signal". */
    (void)finder_model_on_button(&model, FINDER_BTN_OK, 1020U); /* back to scan */
    model.target_filter_valid = false;
    model.target_ever_seen = false;
    model.target_locked = true;
    model.state = FINDER_STATE_TRACK;
    model.target_locked_ms = 1020U;
    finder_model_view(&model, &view, 1500U);
    assert(view.target_present);
    assert(view.target_acquiring);
}

int main(void)
{
    check_list_stays_still();
    check_cursor_follows_device();
    check_tracking_survives_gaps_and_crowding();
    check_private_address_rotation();
    check_grace_period_after_lock();
    puts("test_finder_stability: PASS");
    return 0;
}
