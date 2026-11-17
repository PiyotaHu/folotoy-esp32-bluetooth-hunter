#include "finder_model.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define SETTINGS_CUE_MS 280U

/* Resolvable private addresses rotate. Only accept a same-name, different-MAC
 * report after the original address has been quiet for this long, so two
 * identically named devices nearby cannot hijack a locked target. */
#define FINDER_RPA_QUIET_MS 2500U
/* A full list only replaces a row after it has been quiet for this long, or
 * when the newcomer is clearly stronger. */
#define FINDER_EVICT_QUIET_MS 3000U
#define FINDER_EVICT_GAIN_DB 6

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

/* Matching also refreshes the stored address when a private address rotates.
 * *rebonded tells the caller that preferences must be persisted again. */
static bool target_matches(finder_model_t *model,
                           const ble_scan_report_t *report,
                           uint32_t now_ms, bool *rebonded)
{
    const finder_target_t *target = &model->target;
    bool accept = false;

    if (rebonded != NULL) {
        *rebonded = false;
    }
    if (!model->target_locked) {
        return false;
    }
    if (addr_equal(target->addr, report->addr) &&
        target->addr_type == report->addr_type) {
        return true;
    }
    if (target_name_matches(target, report)) {
        if (model->prefs.match_by_name ||
            (uint32_t)(now_ms - model->target_last_seen_ms) >
                FINDER_RPA_QUIET_MS) {
            accept = true;
        }
    }
    if (accept && !addr_equal(target->addr, report->addr)) {
        memcpy(model->target.addr, report->addr, sizeof(model->target.addr));
        model->target.addr_type = report->addr_type;
        if (rebonded != NULL) {
            *rebonded = true;
        }
    }
    return accept;
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

/* A crowded room produces far more than 12 advertisers. Replacing the oldest
 * row on every new packet made the list flicker and could evict the locked
 * target. Only evict a stale row or a clearly weaker one, never the target. */
static finder_device_t *device_for_insert(finder_model_t *model,
                                          const ble_scan_report_t *report,
                                          uint32_t now_ms)
{
    if (model->count < FINDER_MAX_DEVICES) {
        return &model->devices[model->count++];
    }
    finder_device_t *victim = NULL;
    int8_t weakest = INT8_MAX;

    for (uint8_t i = 0U; i < model->count; ++i) {
        finder_device_t *device = &model->devices[i];

        if (model->target_locked &&
            addr_equal(model->target.addr, device->addr)) {
            continue;
        }
        if (device->sort_rssi < weakest) {
            weakest = device->sort_rssi;
        }
        if (victim == NULL || device->last_seen_ms < victim->last_seen_ms) {
            victim = device;
        }
    }
    if (victim == NULL) {
        return NULL;
    }
    bool stale = (uint32_t)(now_ms - victim->last_seen_ms) >=
                 FINDER_EVICT_QUIET_MS;

    if (!stale && report->rssi <= weakest + FINDER_EVICT_GAIN_DB) {
        return NULL;
    }
    /* When everything is fresh, drop the weakest row rather than the oldest. */
    if (!stale && victim->sort_rssi > weakest) {
        for (uint8_t i = 0U; i < model->count; ++i) {
            finder_device_t *device = &model->devices[i];

            if (model->target_locked &&
                addr_equal(model->target.addr, device->addr)) {
                continue;
            }
            if (device->sort_rssi == weakest) {
                victim = device;
                break;
            }
        }
    }
    return victim;
}

static void init_signal_for(finder_model_t *model, signal_filter_t *filter)
{
    signal_config_t config;

    signal_config_from_profile(&config, model->prefs.profile);
    signal_filter_init(filter, &config);
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
    init_signal_for(model, &model->target_filter);
    model->target_hold_rssi = INT8_MIN;
    model->target_hold_level = FINDER_LEVEL_OUT;
    if (target != NULL && has_target) {
        model->target = *target;
        model->target_locked = true;
        model->state = FINDER_STATE_TRACK;
        model->target_locked_ms = now_ms;
        model->target_last_seen_ms = now_ms;
    } else {
        model->state = FINDER_STATE_SCAN;
    }
    model->settings_return_state = model->state;
    model->setting_focus = FINDER_SETTING_SOUND;
    model->reported_level = FINDER_LEVEL_OUT;
    model->reported_trend = FINDER_TREND_STABLE;
    model->last_cue_ms = now_ms;
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
            init_signal_for(model, &model->devices[i].filter);
        }
        init_signal_for(model, &model->target_filter);
        model->target_filter_valid = false;
        model->target_hold_rssi = INT8_MIN;
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

/* name[] is longer than label[]. An explicit bounded copy keeps the string
 * terminated without tripping -Wformat-truncation in strict CI builds. */
static void copy_label(char *dest, size_t dest_size, const char *src)
{
    size_t i = 0U;

    while (i + 1U < dest_size && src[i] != '\0') {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

/* Advertising data and scan-response data arrive as separate reports. Merge
 * whichever identity is most informative instead of keeping the first one. */
static void refresh_identity(finder_device_t *device,
                             const ble_scan_report_t *report)
{
    if (report_has_name(report) &&
        (!device->has_name ||
         strlen(report->name) > strlen(device->name))) {
        memcpy(device->name, report->name, sizeof(device->name));
        device->has_name = true;
        copy_label(device->label, sizeof(device->label), device->name);
        device->label_inferred = false;
    }
    if (!device->has_name && report->label[0] != '\0' &&
        (device->label[0] == '\0' ||
         (report->kind != FINDER_KIND_UNKNOWN &&
          device->kind == FINDER_KIND_UNKNOWN))) {
        memcpy(device->label, report->label, sizeof(device->label));
        device->label[sizeof(device->label) - 1U] = '\0';
        device->kind = report->kind;
        device->label_inferred = report->label_inferred;
    }
}

static void update_device(finder_device_t *device,
                          const ble_scan_report_t *report, uint32_t now_ms)
{
    int8_t smoothed;
    int delta;

    refresh_identity(device, report);
    device->last_seen_ms = now_ms;
    ++device->packets;
    (void)signal_filter_add(&device->filter, report->rssi, now_ms);

    /* The ordering key is frozen and only moves for a large, sustained
     * change. Live RSSI jitter can no longer reshuffle the list. */
    smoothed = signal_filter_value(&device->filter);
    if (smoothed == INT8_MIN ||
        (uint32_t)(now_ms - device->sort_updated_ms) <
            FINDER_SORT_MIN_INTERVAL_MS) {
        return;
    }
    delta = (int)smoothed - (int)device->sort_rssi;
    if (delta > FINDER_SORT_HYSTERESIS_DB ||
        delta < -FINDER_SORT_HYSTERESIS_DB) {
        device->sort_rssi = smoothed;
        device->sort_updated_ms = now_ms;
    }
}

static void refresh_target_from_device(finder_model_t *model,
                                       const finder_device_t *device)
{
    if (!model->target_locked) {
        return;
    }
    if (!addr_equal(model->target.addr, device->addr) ||
        model->target.addr_type != device->addr_type) {
        return;
    }
    if (device->has_name &&
        (!model->target.has_name ||
         strlen(device->name) > strlen(model->target.name))) {
        memcpy(model->target.name, device->name, sizeof(model->target.name));
        model->target.has_name = true;
    }
    if (device->label[0] != '\0') {
        memcpy(model->target.label, device->label, sizeof(model->target.label));
        model->target.label_inferred = device->label_inferred;
        model->target.kind = device->kind;
    }
}

finder_actions_t finder_model_on_report(finder_model_t *model,
                                        const ble_scan_report_t *report,
                                        uint32_t now_ms)
{
    finder_device_t *device;
    finder_actions_t actions = FINDER_ACTION_UI_CHANGED;
    bool matched;
    bool rebonded = false;

    if (model == NULL || report == NULL) {
        return FINDER_ACTION_NONE;
    }
    device = find_device(model, report);
    if (device == NULL) {
        device = device_for_insert(model, report, now_ms);
        if (device == NULL) {
            return FINDER_ACTION_NONE;
        }
        memset(device, 0, sizeof(*device));
        memcpy(device->addr, report->addr, sizeof(device->addr));
        device->addr_type = report->addr_type;
        device->first_seen_ms = now_ms;
        device->sort_rssi = report->rssi;
        device->sort_updated_ms = now_ms;
        init_signal_for(model, &device->filter);
    }
    update_device(device, report, now_ms);
    refresh_target_from_device(model, device);

    matched = target_matches(model, report, now_ms, &rebonded);
    if (!matched) {
        return actions;
    }
    if (rebonded) {
        actions |= FINDER_ACTION_SAVE;
    }
    /* Feed an independent filter. Tracking no longer depends on the target's
     * row surviving in the 12-slot nearby list. */
    if (!model->target_filter_valid) {
        init_signal_for(model, &model->target_filter);
        model->target_filter_valid = true;
    }
    (void)signal_filter_add(&model->target_filter, report->rssi, now_ms);
    model->target_last_seen_ms = now_ms;
    model->target_ever_seen = true;
    model->target_hold_rssi = signal_filter_value(&model->target_filter);
    model->target_hold_level =
        signal_filter_peek_level(&model->target_filter, now_ms);
    if (model->target.label[0] == '\0' && report->label[0] != '\0') {
        memcpy(model->target.label, report->label, sizeof(model->target.label));
        model->target.label_inferred = report->label_inferred;
        model->target.kind = report->kind;
    }
    if (model->state != FINDER_STATE_TRACK) {
        return actions;
    }
    {
        finder_level_t level =
            signal_filter_peek_level(&model->target_filter, now_ms);
        finder_trend_t trend;
        int8_t delta = 0;

        trend = signal_filter_trend(&model->target_filter, now_ms, &delta);
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
    }
    return actions;
}

static void lock_selected(finder_model_t *model, uint32_t now_ms)
{
    finder_ui_model_t view;

    finder_model_view(model, &view, now_ms);
    if (view.view_count == 0U || view.cursor >= view.view_count) {
        return;
    }
    const finder_device_view_t *selected = &view.devices[view.cursor];
    memset(&model->target, 0, sizeof(model->target));
    memcpy(model->target.addr, selected->addr, sizeof(model->target.addr));
    model->target.addr_type = selected->addr_type;
    if (selected->has_name) {
        memcpy(model->target.name, selected->name, sizeof(model->target.name));
        model->target.has_name = true;
    }
    if (selected->label[0] != '\0') {
        memcpy(model->target.label, selected->label,
               sizeof(model->target.label));
        model->target.label_inferred = selected->label_inferred;
        model->target.kind = selected->kind;
    }
    model->target_locked = true;
    model->state = FINDER_STATE_TRACK;
    model->target_locked_ms = now_ms;

    /* Seed tracking with the samples already collected on the scan screen.
     * Without this, the tracking page starts empty and shows "no signal"
     * until the next advertisement happens to arrive. */
    for (uint8_t i = 0U; i < model->count; ++i) {
        const finder_device_t *device = &model->devices[i];

        if (addr_equal(device->addr, model->target.addr) &&
            device->addr_type == model->target.addr_type) {
            model->target_filter = device->filter;
            model->target_filter_valid = true;
            model->target_last_seen_ms = device->last_seen_ms;
            model->target_ever_seen = true;
            model->target_hold_rssi = signal_filter_value(&model->target_filter);
            model->target_hold_level =
                signal_filter_peek_level(&model->target_filter, now_ms);
            model->reported_level = model->target_hold_level;
            break;
        }
    }
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

static void pin_cursor_at(finder_model_t *model, const finder_ui_model_t *view,
                          uint8_t index)
{
    if (index >= view->view_count) {
        return;
    }
    const finder_device_view_t *selected = &view->devices[index];

    memcpy(model->cursor_addr, selected->addr, sizeof(model->cursor_addr));
    model->cursor_addr_type = selected->addr_type;
    model->cursor_pinned = true;
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
                view.cursor, (uint16_t)(view.view_count - 1U),
                button == FINDER_BTN_UP ? -1 : 1);
            pin_cursor_at(model, &view, model->cursor);
            return FINDER_ACTION_UI_CHANGED;
        }
        if (button == FINDER_BTN_OK) {
            model->cursor = view.cursor;
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
        model->cursor_pinned = false;
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
    bool fresh;

    if (model == NULL) {
        return FINDER_ACTION_NONE;
    }
    if (model->state != FINDER_STATE_TRACK || !model->target_locked ||
        !model->target_filter_valid) {
        return FINDER_ACTION_NONE;
    }
    fresh = signal_filter_fresh(&model->target_filter, now_ms);
    if (fresh) {
        level = signal_filter_tick(&model->target_filter, now_ms);
    } else if ((uint32_t)(now_ms - model->target_last_seen_ms) <=
               FINDER_TARGET_HOLD_MS) {
        level = model->target_hold_level;  /* keep the last reading visible */
    } else {
        level = FINDER_LEVEL_OUT;
    }
    if (level != model->reported_level) {
        model->reported_level = level;
        model->level_started_ms = now_ms;
        actions |= FINDER_ACTION_UI_CHANGED | FINDER_ACTION_CUE_LEVEL;
    }
    if (level == FINDER_LEVEL_OUT || !fresh) {
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

/* Stable ordering: frozen signal key, then discovery order, then address.
 * No term here changes with a single noisy RSSI sample. */
static int compare_devices(const finder_model_t *model, uint8_t left,
                           uint8_t right)
{
    const finder_device_t *a = &model->devices[left];
    const finder_device_t *b = &model->devices[right];

    if (a->sort_rssi > b->sort_rssi) {
        return -1;
    }
    if (a->sort_rssi < b->sort_rssi) {
        return 1;
    }
    if (a->first_seen_ms < b->first_seen_ms) {
        return -1;
    }
    if (a->first_seen_ms > b->first_seen_ms) {
        return 1;
    }
    return memcmp(a->addr, b->addr, FINDER_ADDR_LEN);
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

        while (position > 0U &&
               compare_devices(model, order[position - 1U], value) > 0) {
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
    /* A pinned selection follows its device even if rows move or a row
     * disappears. This is what makes OK lock the highlighted device. */
    if (count > 0U && model->cursor_pinned) {
        for (uint8_t i = 0U; i < count; ++i) {
            const finder_device_t *device = &model->devices[order[i]];

            if (addr_equal(device->addr, model->cursor_addr) &&
                device->addr_type == model->cursor_addr_type) {
                view->cursor = i;
                break;
            }
        }
    }
    if (count > 0U && view->cursor >= count) {
        view->cursor = (uint8_t)(count - 1U);
    }
    view->scroll_offset = view->cursor >= FINDER_LIST_ROWS
                              ? (uint8_t)(view->cursor - FINDER_LIST_ROWS + 1U) : 0U;
    view->target = model->target;
    view->target_valid = model->target_locked;
    view->target_present = false;
    view->target_rssi = INT8_MIN;
    view->target_locked_age_ms = (uint32_t)(now_ms - model->target_locked_ms);

    for (uint8_t i = 0U; i < count; ++i) {
        const finder_device_t *device = &model->devices[order[i]];
        finder_device_view_t *out = &view->devices[i];

        memcpy(out->addr, device->addr, sizeof(out->addr));
        out->addr_type = device->addr_type;
        if (device->has_name) {
            memcpy(out->name, device->name, sizeof(out->name));
            out->has_name = true;
        }
        memcpy(out->label, device->label, sizeof(out->label));
        out->kind = device->kind;
        out->label_inferred = device->label_inferred;
        out->raw_rssi = device->filter.sample_count > 0U
                            ? device->filter.samples[(device->filter.sample_index +
                                                      SIGNAL_WINDOW - 1U) % SIGNAL_WINDOW]
                            : INT8_MIN;
        out->filtered_rssi = signal_filter_value(&device->filter);
        out->first_seen_ms = device->first_seen_ms;
        out->last_seen_ms = device->last_seen_ms;
        out->packets = device->packets;
    }

    /* Tracking reads its own filter, so it survives list eviction entirely. */
    if (model->target_locked) {
        uint32_t age = (uint32_t)(now_ms - model->target_last_seen_ms);
        bool fresh = model->target_filter_valid &&
                     signal_filter_fresh(&model->target_filter, now_ms);
        bool holding = model->target_ever_seen && age <= FINDER_TARGET_HOLD_MS;
        bool acquiring = !model->target_ever_seen &&
                         view->target_locked_age_ms <= FINDER_TARGET_GRACE_MS;

        view->last_seen_age_ms = age;
        view->target_acquiring = acquiring;
        view->target_present = fresh || holding || acquiring;
        if (fresh) {
            view->target_rssi = signal_filter_value(&model->target_filter);
            view->level = signal_filter_peek_level(&model->target_filter,
                                                  now_ms);
            view->trend = signal_filter_trend(&model->target_filter, now_ms,
                                              &view->trend_delta_db);
        } else if (holding || acquiring) {
            view->target_rssi = model->target_hold_rssi;
            view->level = model->target_hold_level;
            view->trend = FINDER_TREND_STABLE;
        } else {
            view->level = FINDER_LEVEL_OUT;
            view->trend = FINDER_TREND_STABLE;
            view->last_seen_age_ms = UINT32_MAX;
        }
    } else {
        view->level = FINDER_LEVEL_OUT;
        view->trend = FINDER_TREND_STABLE;
        view->target_rssi = INT8_MIN;
        view->last_seen_age_ms = UINT32_MAX;
    }
}
