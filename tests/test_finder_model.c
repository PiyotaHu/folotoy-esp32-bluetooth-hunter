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
    report.timestamp_ms = 0U;
    return report;
}

static void emit(finder_model_t *model, const uint8_t addr[6], int8_t rssi,
                 uint32_t now)
{
    ble_scan_report_t report = make_report(addr, rssi);

    report.timestamp_ms = now;
    (void)finder_model_on_report(model, &report, now);
}

int main(void)
{
    static const uint8_t a[6] = {0xAA, 1, 2, 3, 4, 5};
    static const uint8_t b[6] = {0xBB, 1, 2, 3, 4, 5};
    finder_preferences_t prefs;
    finder_model_t model;
    finder_ui_model_t view;
    finder_target_t target;
    uint32_t now = 1000U;

    finder_preferences_defaults(&prefs);
    finder_model_init(&model, &prefs, NULL, false, now);
    assert(model.state == FINDER_STATE_SCAN);

    emit(&model, a, -80, now);
    emit(&model, b, -60, now + 10U);
    finder_model_view(&model, &view, now + 20U);
    assert(view.view_count == 2U);
    assert(memcmp(view.devices[0].addr, b, sizeof(a)) == 0);

    (void)finder_model_on_button(&model, FINDER_BTN_OK, now + 30U);
    assert(model.state == FINDER_STATE_TRACK);
    target = finder_model_target(&model);
    assert(memcmp(target.addr, b, sizeof(a)) == 0);

    emit(&model, a, -75, now + 100U);
    for (uint32_t t = now + 110U; t < now + 1500U; t += 100U) {
        emit(&model, b, -35, t);
    }
    finder_model_view(&model, &view, now + 1550U);
    assert(view.target_present);
    assert(view.level == FINDER_LEVEL_VERY_CLOSE);

    (void)finder_model_on_button(&model, FINDER_BTN_DOWN, now + 130U);
    assert(model.prefs.volume == 60U);
    (void)finder_model_on_button(&model, FINDER_BTN_OK_LONG, now + 140U);
    assert(model.state == FINDER_STATE_SETTINGS);
    (void)finder_model_on_button(&model, FINDER_BTN_OK, now + 150U);
    assert(!model.prefs.sound_enabled);
    (void)finder_model_on_button(&model, FINDER_BTN_OK_LONG, now + 160U);
    assert(model.state == FINDER_STATE_TRACK);

    emit(&model, b, -45, now + 1600U);
    (void)finder_model_tick(&model, now + 1600U);
    finder_model_view(&model, &view, now + 7000U);
    assert(!view.target_present);
    assert(view.level == FINDER_LEVEL_OUT);
    assert(view.target_rssi == INT8_MIN);

    puts("test_finder_model: PASS");
    return 0;
}
