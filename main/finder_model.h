#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_report.h"
#include "finder_types.h"
#include "signal_filter.h"

/* Devices stay in the nearby list for this long without a fresh packet.
 * Consumer BLE gear often advertises at 1-3 s (some only when woken), so a
 * short window made rows appear/disappear constantly. */
#define FINDER_DEVICE_TIMEOUT_MS 10000U

/* After locking, give the radio time to catch the next advertisement before
 * the UI is allowed to say "no signal". */
#define FINDER_TARGET_GRACE_MS 4000U

/* Once a packet is missed, hold the last reading instead of blanking the UI. */
#define FINDER_TARGET_HOLD_MS 5000U

/* The list order is frozen per device. It only changes when its smoothed
 * signal has moved by this much AND the change has been sustained. */
#define FINDER_SORT_HYSTERESIS_DB 12
#define FINDER_SORT_MIN_INTERVAL_MS 3000U

typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    char name[FINDER_NAME_LEN];
    char label[FINDER_LABEL_LEN];
    finder_device_kind_t kind;
    bool has_name;
    bool label_inferred;
    signal_filter_t filter;
    int8_t sort_rssi;          /* frozen ordering key, not live RSSI */
    uint32_t sort_updated_ms;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint16_t packets;
} finder_device_t;

typedef struct {
    finder_device_t devices[FINDER_MAX_DEVICES];
    uint8_t count;
    finder_target_t target;
    bool target_locked;
    /* Tracking state lives here, independent of the nearby list. A target can
     * never lose its signal just because a crowded list evicted its row. */
    signal_filter_t target_filter;
    bool target_filter_valid;
    uint32_t target_last_seen_ms;
    uint32_t target_locked_ms;
    int8_t target_hold_rssi;
    finder_level_t target_hold_level;
    bool target_ever_seen;
    /* The selection follows a device, never a moving row index. */
    uint8_t cursor_addr[FINDER_ADDR_LEN];
    uint8_t cursor_addr_type;
    bool cursor_pinned;
    finder_state_t state;
    finder_state_t settings_return_state;
    finder_setting_t setting_focus;
    finder_preferences_t prefs;
    uint8_t cursor;
    uint8_t scroll_offset;
    uint32_t last_cue_ms;
    uint32_t level_started_ms;
    finder_level_t reported_level;
    finder_trend_t reported_trend;
    bool found_cued;
} finder_model_t;

void finder_preferences_defaults(finder_preferences_t *prefs);
void finder_model_init(finder_model_t *model,
                       const finder_preferences_t *prefs,
                       const finder_target_t *target,
                       bool has_target,
                       uint32_t now_ms);
void finder_model_set_preferences(finder_model_t *model,
                                  const finder_preferences_t *prefs,
                                  uint32_t now_ms);
const finder_preferences_t *finder_model_preferences(const finder_model_t *model);
finder_target_t finder_model_target(const finder_model_t *model);
finder_actions_t finder_model_on_report(finder_model_t *model,
                                        const ble_scan_report_t *report,
                                        uint32_t now_ms);
finder_actions_t finder_model_on_button(finder_model_t *model,
                                        finder_button_t button,
                                        uint32_t now_ms);
finder_actions_t finder_model_tick(finder_model_t *model, uint32_t now_ms);
void finder_model_view(finder_model_t *model, finder_ui_model_t *view,
                       uint32_t now_ms);
uint32_t finder_model_cue_interval_ms(finder_level_t level);
