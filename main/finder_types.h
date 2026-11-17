#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FINDER_MAX_DEVICES 12U
#define FINDER_LIST_ROWS 5U
#define FINDER_NAME_LEN 32U
#define FINDER_LABEL_LEN 24U
#define FINDER_ADDR_LEN 6U

/* Best-effort device category inferred from advertising data. The label shown
 * in the list is ASCII so it never depends on the bundled CJK glyph subset. */
typedef enum {
    FINDER_KIND_UNKNOWN = 0,
    FINDER_KIND_EARBUDS,
    FINDER_KIND_PHONE,
    FINDER_KIND_WATCH,
    FINDER_KIND_TAG,
    FINDER_KIND_COMPUTER,
    FINDER_KIND_SPEAKER,
    FINDER_KIND_BEACON,
    FINDER_KIND_OTHER,
} finder_device_kind_t;

typedef enum {
    FINDER_STATE_SCAN = 0,
    FINDER_STATE_TRACK,
    FINDER_STATE_SETTINGS,
} finder_state_t;

typedef enum {
    FINDER_LEVEL_OUT = 0,
    FINDER_LEVEL_FAR,
    FINDER_LEVEL_NEAR,
    FINDER_LEVEL_CLOSE,
    FINDER_LEVEL_VERY_CLOSE,
    FINDER_LEVEL_COUNT,
} finder_level_t;

typedef enum {
    FINDER_TREND_STABLE = 0,
    FINDER_TREND_CLOSER,
    FINDER_TREND_AWAY,
} finder_trend_t;

typedef enum {
    FINDER_PROFILE_FAST = 0,
    FINDER_PROFILE_NORMAL,
    FINDER_PROFILE_STABLE,
    FINDER_PROFILE_COUNT,
} finder_profile_t;

typedef enum {
    FINDER_SETTING_SOUND = 0,
    FINDER_SETTING_VOLUME,
    FINDER_SETTING_MATCH_NAME,
    FINDER_SETTING_PROFILE,
    FINDER_SETTING_COUNT,
} finder_setting_t;

typedef enum {
    FINDER_BTN_UP = 0,
    FINDER_BTN_DOWN,
    FINDER_BTN_OK,
    FINDER_BTN_OK_LONG,
} finder_button_t;

enum {
    FINDER_ACTION_NONE = 0U,
    FINDER_ACTION_UI_CHANGED = 1U << 0,
    FINDER_ACTION_CUE_LEVEL = 1U << 1,
    FINDER_ACTION_CUE_CLOSER = 1U << 2,
    FINDER_ACTION_CUE_AWAY = 1U << 3,
    FINDER_ACTION_CUE_LOCK = 1U << 4,
    FINDER_ACTION_CUE_FOUND = 1U << 5,
    FINDER_ACTION_SAVE = 1U << 6,
};

typedef uint32_t finder_actions_t;

typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    char name[FINDER_NAME_LEN];
    char label[FINDER_LABEL_LEN];
    finder_device_kind_t kind;
    bool has_name;
    bool label_inferred;
} finder_target_t;

typedef struct {
    bool sound_enabled;
    uint8_t volume;
    bool match_by_name;
    finder_profile_t profile;
} finder_preferences_t;

typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    char name[FINDER_NAME_LEN];
    char label[FINDER_LABEL_LEN];
    finder_device_kind_t kind;
    bool has_name;
    bool label_inferred;
    int8_t raw_rssi;
    int8_t filtered_rssi;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint16_t packets;
} finder_device_view_t;

typedef struct {
    finder_state_t state;
    finder_state_t settings_return_state;
    finder_setting_t setting_focus;
    uint8_t view_count;
    uint8_t cursor;
    uint8_t scroll_offset;
    finder_target_t target;
    bool target_valid;
    bool target_present;
    bool target_acquiring;
    int8_t target_rssi;
    finder_level_t level;
    finder_trend_t trend;
    int8_t trend_delta_db;
    uint32_t last_seen_age_ms;
    uint32_t target_locked_age_ms;
    int16_t battery;
    finder_preferences_t prefs;
    finder_device_view_t devices[FINDER_MAX_DEVICES];
} finder_ui_model_t;
