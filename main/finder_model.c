#include "finder_model.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define SETTINGS_CUE_MS 280U

void finder_preferences_defaults(finder_preferences_t *prefs)
{
    if (prefs == NULL) {
        return;
    }
    prefs->sound_enabled = true;
    prefs->volume = 70U;
    prefs->match_by_name = false;
    prefs->profile = FINDER_PROFILE_NORMAL;
}

static bool addr_equal(const uint8_t *left, const uint8_t *right)
{
    return memcmp(left, right, FINDER_ADDR_LEN) == 0;
}

static bool report_has_name(const ble_scan_report_t *report)
{
    return report->has_name && report->name[0] != '\0';
}

static bool target_name_matches(const finder_target_t *target,
                                const ble_scan_report_t *report)
{
    return target->has_name && report_has_name(report) &&
           strcmp(target->name, report->name) == 0;
}

static bool target_matches(const finder_model_t *model,
                           const ble_scan_report_t *report)
{
    const finder_target_t *target = &model->target;

    if (!model->target_locked) {
        return false;
    }
    if (addr_equal(target->addr, report->addr) &&
        target->addr_type == report->addr_type) {
        return true;
    }
    return model->prefs.match_by_name && target_name_matches(target, report);
}

static finder_device_t *find_device(finder_model_t *model,
                                    const ble_scan_report_t *report)
{
    for (uint8_t i = 0U; i < model->count; ++i) {
        finder_device_t *device = &model->devices[i];

        if (addr_equal(device->addr, report->addr) &&
            device->addr_type == report->addr_type) {
            return device;
        }
    }
    return NULL;
}

static finder_device_t *device_for_insert(finder_model_t *model)
{
    if (model->count < FINDER_MAX_DEVICES) {
        return &model->devices[model->count++];
    }
    finder_device_t *oldest = &model->devices[0];

    for (uint8_t i = 1U; i < model->count; ++i) {
        if (model->devices[i].last_seen_ms < oldest->last_seen_ms) {
            oldest = &model->devices[i];
        }
    }
    return oldest;
}

static void init_signal_for(finder_model_t *model, finder_device_t *device)
{
    signal_config_t config;

    signal_config_from_profile(&config, model->prefs.profile);
    signal_filter_init(&device->filter, &config);
}

void finder_model_init(finder_model_t *model,
                       const finder_preferences_t *prefs,
                       const finder_target_t *target,
                       bool has_target,
                       uint32_t now_ms)
{
    memset(model, 0, sizeof(*model));
    finder_preferences_defaults(&model->prefs);
    if (prefs != NULL) {
        model->prefs = *prefs;
    }
    if (target != NULL && has_target) {
        model->target = *target;
        model->target_locked = true;
        model->state = FINDER_STATE_TRACK;
    } else {
        model->state = FINDER_STATE_SCAN;
    }
    model->settings_return_state = model->state;
    model->setting_focus = FINDER_SETTING_SOUND;
    model->reported_level = FINDER_LEVEL_OUT;
    model->reported_trend = FINDER_TREND_STABLE;
    model->last_cue_ms = now_ms;
    (void)now_ms;
}

void finder_model_set_preferences(finder_model_t *model,
                                  const finder_preferences_t *prefs,
                                  uint32_t now_ms)
{
    bool profile_changed;

    if (model == NULL || prefs == NULL) {
        return;
    }
    profile_changed = model->prefs.profile != prefs->profile;
    model->prefs = *prefs;
    if (profile_changed) {
        for (uint8_t i = 0U; i < model->count; ++i) {
            init_signal_for(model, &model->devices[i]);
        }
        model->found_cued = false;
        model->level_started_ms = now_ms;
    }
}

const finder_preferences_t *finder_model_preferences(const finder_model_t *model)
{
    return model == NULL ? NULL : &model->prefs;
}

finder_target_t finder_model_target(const finder_model_t *model)
{
    finder_target_t empty;

    memset(&empty, 0, sizeof(empty));
    return model == NULL || !model->target_locked ? empty : model->target;
}

static void update_device(finder_device_t *device,
                          const ble_scan_report_t *report, uint32_t now_ms)
{
    bool had_name = device->has_name;

    if (!had_name && report_has_name(report)) {
        memcpy(device->name, report->name, sizeof(device->name));
        device->has_name = true;
    }
    device->last_seen_ms = now_ms;
    ++device->packets;
    (void)signal_filter_add(&device->filter, report->rssi, now_ms);
}

static void refresh_target_from_device(finder_model_t *model,
                                       const finder_device_t *device)
{
    if (!model->target_locked) {
        return;
    }
    if (addr_equal(model->target.addr, device->addr) &&
        model->target.addr_type == device->addr_type) {
        if (!model->target.has_name && device->has_name) {
            memcpy(model->target.name, device->name, sizeof(model->target.name));
            model->target.has_name = true;
        }
    }
}

finder_actions_t finder_model_on_report(finder_model_t *model,
                                        const ble_scan_report_t *report,
                                        uint32_t now_ms)
{
    finder_device_t *device;
    finder_level_t level;
    finder_trend_t trend;
    int8_t delta = 0;
    finder_actions_t actions = FINDER_ACTION_UI_CHANGED;
    bool matched;

    if (model == NULL || report == NULL) {
        return FINDER_ACTION_NONE;
    }
    device = find_device(model, report);
    if (device == NULL) {
        device = device_for_insert(model);
        memset(device, 0, sizeof(*device));
        memcpy(device->addr, report->addr, sizeof(device->addr));
        device->addr_type = report->addr_type;
        device->first_seen_ms = now_ms;
        init_signal_for(model, device);
    }
    update_device(device, report, now_ms);
    refresh_target_from_device(model, device);

    matched = target_matches(model, report);
    if (!matched || model->state != FINDER_STATE_TRACK) {
        return actions;
    }

    // Keep the freshest address for name-matched targets with rotating MACs.
    if (model->prefs.match_by_name && !addr_equal(model->target.addr, report->addr)) {
        memcpy(model->target.addr, report->addr, sizeof(model->target.addr));
        model->target.addr_type = report->addr_type;
        actions |= FINDER_ACTION_SAVE;
    }

    level = signal_filter_value(&device->filter) == INT8_MIN
                ? FINDER_LEVEL_OUT : signal_filter_tick(&device->filter, now_ms);
    trend = signal_filter_trend(&device->filter, now_ms, &delta);
    if (level != model->reported_level) {
        model->reported_level = level;
        model->level_started_ms = now_ms;
        actions |= FINDER_ACTION_CUE_LEVEL;
        if (level == FINDER_LEVEL_VERY_CLOSE) {
            actions |= FINDER_ACTION_CUE_FOUND;
        }
    }
    if (trend != model->reported_trend) {
        if (trend == FINDER_TREND_CLOSER) {
            actions |= FINDER_ACTION_CUE_CLOSER;
        } else if (trend == FINDER_TREND_AWAY) {
            actions |= FINDER_ACTION_CUE_AWAY;
        }
        model->reported_trend = trend;
    }
    return actions;
}

static void lock_selected(finder_model_t *model, uint32_t now_ms)
{
    finder_ui_model_t view;

    finder_model_view(model, &view, now_ms);
    if (view.view_count == 0U || model->cursor >= view.view_count) {
        return;
    }
    // view.devices keeps the same sorted ordering as the cursor.
    const finder_device_view_t *selected = &view.devices[model->cursor];
    memset(&model->target, 0, sizeof(model->target));
    memcpy(model->target.addr, selected->addr, sizeof(model->target.addr));
    model->target.addr_type = selected->addr_type;
    if (selected->has_name) {
        memcpy(model->target.name, selected->name, sizeof(model->target.name));
        model->target.has_name = true;
    }
    model->target_locked = true;
    model->state = FINDER_STATE_TRACK;
    model->reported_level = FINDER_LEVEL_OUT;
    model->reported_trend = FINDER_TREND_STABLE;
    model->level_started_ms = now_ms;
    model->found_cued = false;
    model->last_cue_ms = now_ms;
}

static uint16_t cycle_u8(uint16_t value, uint16_t max, int direction)
{
    int next = (int)value + direction;

    if (next < 0) {
        return max;
    }
    if ((uint16_t)next > max) {
        return 0U;
    }
    return (uint16_t)next;
}

static finder_actions_t adjust_setting(finder_model_t *model, uint32_t now_ms)
{
    finder_actions_t actions = FINDER_ACTION_UI_CHANGED | FINDER_ACTION_SAVE;

    switch (model->setting_focus) {
    case FINDER_SETTING_SOUND:
        model->prefs.sound_enabled = !model->prefs.sound_enabled;
        break;
    case FINDER_SETTING_VOLUME:
        model->prefs.volume = (uint8_t)cycle_u8(model->prefs.volume, 100U, 10);
        break;
    case FINDER_SETTING_MATCH_NAME:
        model->prefs.match_by_name = !model->prefs.match_by_name;
        break;
    case FINDER_SETTING_PROFILE:
        model->prefs.profile = (finder_profile_t)cycle_u8(
            (uint16_t)model->prefs.profile,
            (uint16_t)(FINDER_PROFILE_COUNT - 1U), 1);
        finder_model_set_preferences(model, &model->prefs, now_ms);
        break;
    default:
        return FINDER_ACTION_NONE;
    }
    return actions;
}

finder_actions_t finder_model_on_button(finder_model_t *model,
                                        finder_button_t button,
                                        uint32_t now_ms)
{
    finder_ui_model_t view;
    if (model == NULL) {
        return FINDER_ACTION_NONE;
    }
    if (button == FINDER_BTN_OK_LONG) {
        if (model->state == FINDER_STATE_SETTINGS) {
            model->state = model->settings_return_state;
        } else {
            model->settings_return_state = model->state;
            model->state = FINDER_STATE_SETTINGS;
        }
        return FINDER_ACTION_UI_CHANGED;
    }

    if (model->state == FINDER_STATE_SETTINGS) {
        if (button == FINDER_BTN_UP || button == FINDER_BTN_DOWN) {
            model->setting_focus = (finder_setting_t)cycle_u8(
                (uint16_t)model->setting_focus,
                (uint16_t)(FINDER_SETTING_COUNT - 1U),
                button == FINDER_BTN_UP ? -1 : 1);
            return FINDER_ACTION_UI_CHANGED;
        }
        if (button == FINDER_BTN_OK) {
            return adjust_setting(model, now_ms);
        }
        return FINDER_ACTION_NONE;
    }

    if (model->state == FINDER_STATE_SCAN) {
        finder_model_view(model, &view, now_ms);
        if (button == FINDER_BTN_UP || button == FINDER_BTN_DOWN) {
            if (view.view_count == 0U) {
                return FINDER_ACTION_NONE;
            }
            model->cursor = (uint8_t)cycle_u8(
                model->cursor, (uint16_t)(view.view_count - 1U),
                button == FINDER_BTN_UP ? -1 : 1);
            return FINDER_ACTION_UI_CHANGED;
        }
        if (button == FINDER_BTN_OK) {
            lock_selected(model, now_ms);
            return FINDER_ACTION_UI_CHANGED | FINDER_ACTION_CUE_LOCK |
                   FINDER_ACTION_SAVE;
        }
        return FINDER_ACTION_NONE;
    }

    // TRACK: up/down are a direct volume shortcut while walking.
    if (button == FINDER_BTN_UP || button == FINDER_BTN_DOWN) {
        int next = (int)model->prefs.volume + (button == FINDER_BTN_UP ? 10 : -10);
        if (next < 0) {
            next = 0;
        } else if (next > 100) {
            next = 100;
        }
        if (next == (int)model->prefs.volume) {
            return FINDER_ACTION_NONE;
        }
        model->prefs.volume = (uint8_t)next;
        return FINDER_ACTION_UI_CHANGED | FINDER_ACTION_SAVE;
    }
    if (button == FINDER_BTN_OK) {
        model->state = FINDER_STATE_SCAN;
        model->cursor = 0U;
        return FINDER_ACTION_UI_CHANGED;
    }
    return FINDER_ACTION_NONE;
}

uint32_t finder_model_cue_interval_ms(finder_level_t level)
{
    switch (level) {
    case FINDER_LEVEL_VERY_CLOSE: return 180U;
    case FINDER_LEVEL_CLOSE: return 360U;
    case FINDER_LEVEL_NEAR: return 720U;
    case FINDER_LEVEL_FAR: return 1500U;
    case FINDER_LEVEL_OUT:
    default: return 3000U;
    }
}

finder_actions_t finder_model_tick(finder_model_t *model, uint32_t now_ms)
{
    finder_actions_t actions = FINDER_ACTION_NONE;
    finder_level_t level;
    int8_t rssi;

    if (model == NULL) {
        return FINDER_ACTION_NONE;
    }
    if (model->state != FINDER_STATE_TRACK || !model->target_locked) {
        return FINDER_ACTION_NONE;
    }
    for (uint8_t i = 0U; i < model->count; ++i) {
        finder_device_t *device = &model->devices[i];
        if (!addr_equal(device->addr, model->target.addr) ||
            device->addr_type != model->target.addr_type) {
            continue;
        }
        rssi = signal_filter_value(&device->filter);
        if (rssi == INT8_MIN) {
            continue;
        }
        level = signal_filter_tick(&device->filter, now_ms);
        if (level != model->reported_level) {
            model->reported_level = level;
            model->level_started_ms = now_ms;
            actions |= FINDER_ACTION_UI_CHANGED | FINDER_ACTION_CUE_LEVEL;
        }
        if (level == FINDER_LEVEL_OUT || !signal_filter_fresh(&device->filter, now_ms)) {
            if (model->reported_trend != FINDER_TREND_STABLE) {
                model->reported_trend = FINDER_TREND_STABLE;
                actions |= FINDER_ACTION_UI_CHANGED;
            }
            if (now_ms - model->last_cue_ms >= 3000U) {
                model->last_cue_ms = now_ms;
                actions |= FINDER_ACTION_CUE_LEVEL;
            }
            return actions;
        }
        if (level == FINDER_LEVEL_VERY_CLOSE &&
            (uint32_t)(now_ms - model->level_started_ms) >= 1200U &&
            !model->found_cued) {
            model->found_cued = true;
            actions |= FINDER_ACTION_CUE_FOUND;
        }
        if (level != FINDER_LEVEL_VERY_CLOSE) {
            model->found_cued = false;
        }
        if ((uint32_t)(now_ms - model->last_cue_ms) >=
            finder_model_cue_interval_ms(level)) {
            model->last_cue_ms = now_ms;
            actions |= FINDER_ACTION_CUE_LEVEL;
        }
        return actions;
    }
    return FINDER_ACTION_NONE;
}

static int compare_devices(const finder_model_t *model, uint8_t left, uint8_t right)
{
    int8_t left_rssi = signal_filter_value(&model->devices[left].filter);
    int8_t right_rssi = signal_filter_value(&model->devices[right].filter);

    if (left_rssi == INT8_MIN) {
        left_rssi = model->devices[left].filter.samples[0];
    }
    if (right_rssi == INT8_MIN) {
        right_rssi = model->devices[right].filter.samples[0];
    }
    if (left_rssi > right_rssi) {
        return -1;
    }
    if (left_rssi < right_rssi) {
        return 1;
    }
    if (model->devices[left].last_seen_ms > model->devices[right].last_seen_ms) {
        return -1;
    }
    if (model->devices[left].last_seen_ms < model->devices[right].last_seen_ms) {
        return 1;
    }
    return 0;
}

void finder_model_view(finder_model_t *model, finder_ui_model_t *view,
                       uint32_t now_ms)
{
    uint8_t order[FINDER_MAX_DEVICES];
    uint8_t count = 0U;

    if (model == NULL || view == NULL) {
        return;
    }
    memset(view, 0, sizeof(*view));
    for (uint8_t i = 0U; i < model->count; ++i) {
        if ((uint32_t)(now_ms - model->devices[i].last_seen_ms) <=
            FINDER_DEVICE_TIMEOUT_MS) {
            order[count++] = i;
        }
    }
    for (uint8_t i = 1U; i < count; ++i) {
        uint8_t value = order[i];
        uint8_t position = i;

        while (position > 0U && compare_devices(model, order[position - 1U], value) > 0) {
            order[position] = order[position - 1U];
            --position;
        }
        order[position] = value;
    }

    view->state = model->state;
    view->settings_return_state = model->settings_return_state;
    view->setting_focus = model->setting_focus;
    view->prefs = model->prefs;
    view->view_count = count;
    view->cursor = model->cursor;
    if (count > 0U && model->cursor >= count) {
        view->cursor = (uint8_t)(count - 1U);
    }
    view->scroll_offset = view->cursor >= FINDER_LIST_ROWS
                              ? (uint8_t)(view->cursor - FINDER_LIST_ROWS + 1U) : 0U;
    view->target = model->target;
    view->target_valid = model->target_locked;
    view->target_present = false;

    for (uint8_t i = 0U; i < count; ++i) {
        const finder_device_t *device = &model->devices[order[i]];
        finder_device_view_t *out = &view->devices[i];

        memcpy(out->addr, device->addr, sizeof(out->addr));
        out->addr_type = device->addr_type;
        if (device->has_name) {
            memcpy(out->name, device->name, sizeof(out->name));
            out->has_name = true;
        }
        out->raw_rssi = device->filter.sample_count > 0U
                            ? device->filter.samples[(device->filter.sample_index +
                                                      SIGNAL_WINDOW - 1U) % SIGNAL_WINDOW]
                            : INT8_MIN;
        out->filtered_rssi = signal_filter_value(&device->filter);
        out->first_seen_ms = device->first_seen_ms;
        out->last_seen_ms = device->last_seen_ms;
        out->packets = device->packets;
        if (model->target_locked &&
            ((addr_equal(model->target.addr, device->addr) &&
              model->target.addr_type == device->addr_type) ||
             (model->prefs.match_by_name && model->target.has_name &&
              device->has_name && strcmp(model->target.name, device->name) == 0))) {
            view->target_present = true;
            view->target_rssi = out->filtered_rssi;
            view->level = signal_filter_tick(&model->devices[order[i]].filter,
                                             now_ms);
            int8_t delta = 0;
            view->trend = signal_filter_trend(&model->devices[order[i]].filter,
                                              now_ms, &delta);
            view->trend_delta_db = delta;
            view->last_seen_age_ms = signal_filter_age_ms(&model->devices[order[i]].filter, now_ms);
        }
    }
    if (!view->target_present) {
        view->level = FINDER_LEVEL_OUT;
        view->trend = FINDER_TREND_STABLE;
        view->target_rssi = INT8_MIN;
        view->last_seen_age_ms = UINT32_MAX;
    }
}
