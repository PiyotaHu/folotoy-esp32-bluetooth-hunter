#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_report.h"
#include "finder_types.h"
#include "signal_filter.h"

#define FINDER_DEVICE_TIMEOUT_MS 5000U

typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    char name[FINDER_NAME_LEN];
    bool has_name;
    signal_filter_t filter;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint16_t packets;
} finder_device_t;

typedef struct {
    finder_device_t devices[FINDER_MAX_DEVICES];
    uint8_t count;
    finder_target_t target;
    bool target_locked;
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
