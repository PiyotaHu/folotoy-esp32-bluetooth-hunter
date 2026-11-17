#include "finder_ui.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"

LV_FONT_DECLARE(finder_font_14);
LV_FONT_DECLARE(finder_font_20);

#define COLOR_BG 0x0B1320
#define COLOR_PANEL 0x131F33
#define COLOR_INK 0xE8F1FF
#define COLOR_MUTED 0x8EA3C0
#define COLOR_CYAN 0x35D0E0
#define COLOR_GREEN 0x3BD47A
#define COLOR_YELLOW 0xFFC84A
#define COLOR_ORANGE 0xFF8A3D
#define COLOR_RED 0xF05A5A

#define ROW_HEIGHT 34
#define LIST_TOP 68

static lv_obj_t *s_screen;
static lv_obj_t *s_scan_group;
static lv_obj_t *s_track_group;
static lv_obj_t *s_settings_group;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_scan_hint;
static lv_obj_t *s_scan_rows[FINDER_LIST_ROWS];
static lv_obj_t *s_scan_names[FINDER_LIST_ROWS];
static lv_obj_t *s_scan_rssi[FINDER_LIST_ROWS];
static lv_obj_t *s_scan_bars[FINDER_LIST_ROWS];
static lv_obj_t *s_track_title;
static lv_obj_t *s_track_subtitle;
static lv_obj_t *s_track_rssi;
static lv_obj_t *s_track_level;
static lv_obj_t *s_track_trend;
static lv_obj_t *s_track_hint;
static lv_obj_t *s_track_bars[FINDER_LEVEL_COUNT];
static lv_obj_t *s_setting_rows[FINDER_SETTING_COUNT];
static lv_obj_t *s_setting_values[FINDER_SETTING_COUNT];
static lv_obj_t *s_setting_hint;
static bool s_initialized;

static lv_obj_t *make_group(void)
{
    lv_obj_t *group = lv_obj_create(s_screen);

    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(group, 0, 0);
    lv_obj_set_size(group, 240, 286);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(group, 0, 0);
    lv_obj_set_style_pad_all(group, 0, 0);
    return group;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, uint32_t color,
                       lv_align_t align, int x, int y)
{
    lv_obj_t *obj = lv_label_create(parent);

    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, align, x, y);
    return obj;
}

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static void header(void)
{
    block(s_screen, 0, 0, 240, 46, COLOR_PANEL);
    label(s_screen, "蓝牙寻物探测器", &finder_font_20, COLOR_CYAN,
          LV_ALIGN_TOP_LEFT, 12, 11);
    s_battery_label = label(s_screen, "--%", &finder_font_14, COLOR_MUTED,
                            LV_ALIGN_TOP_RIGHT, -12, 15);
}

static void make_scan_screen(void)
{
    static const char *empty = "扫描中，附近还没有蓝牙广播…";

    s_scan_group = make_group();
    label(s_scan_group, "附近设备", &finder_font_14, COLOR_MUTED,
          LV_ALIGN_TOP_LEFT, 12, 52);
    for (uint8_t i = 0U; i < FINDER_LIST_ROWS; ++i) {
        int y = LIST_TOP + (int)i * ROW_HEIGHT;

        s_scan_rows[i] = block(s_scan_group, 8, y, 224, 30, COLOR_PANEL);
        s_scan_names[i] = label(s_scan_rows[i], "—", &finder_font_14,
                                COLOR_INK, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_width(s_scan_names[i], 128);
        s_scan_rssi[i] = label(s_scan_rows[i], "", &finder_font_14,
                               COLOR_MUTED, LV_ALIGN_RIGHT_MID, -8, 0);
        s_scan_bars[i] = block(s_scan_rows[i], 132, 11, 2, 8, COLOR_CYAN);
    }
    s_scan_hint = label(s_scan_group, empty, &finder_font_14,
                        COLOR_MUTED, LV_ALIGN_TOP_MID, 0, 250);
}

static void make_track_screen(void)
{
    static const int bar_heights[] = {12, 20, 28, 36, 44};

    s_track_group = make_group();
    label(s_track_group, "正在追踪", &finder_font_14, COLOR_MUTED,
          LV_ALIGN_TOP_LEFT, 12, 52);
    s_track_title = label(s_track_group, "等待目标", &finder_font_20,
                          COLOR_INK, LV_ALIGN_TOP_LEFT, 12, 70);
    lv_obj_set_width(s_track_title, 216);
    s_track_subtitle = label(s_track_group, "", &finder_font_14,
                             COLOR_MUTED, LV_ALIGN_TOP_LEFT, 12, 96);
    lv_obj_set_width(s_track_subtitle, 216);

    block(s_track_group, 12, 124, 216, 74, COLOR_PANEL);
    s_track_rssi = label(s_track_group, "--", &finder_font_20,
                         COLOR_INK, LV_ALIGN_TOP_LEFT, 24, 136);
    label(s_track_group, "dBm", &finder_font_14, COLOR_MUTED,
          LV_ALIGN_TOP_LEFT, 122, 168);
    s_track_level = label(s_track_group, "未发现", &finder_font_20,
                          COLOR_MUTED, LV_ALIGN_TOP_RIGHT, -24, 142);
    s_track_trend = label(s_track_group, "", &finder_font_14,
                          COLOR_MUTED, LV_ALIGN_TOP_LEFT, 24, 181);

    for (uint8_t i = 0U; i < FINDER_LEVEL_COUNT; ++i) {
        s_track_bars[i] = block(s_track_group, 22 + (int)i * 40,
                                220 - bar_heights[i], 30, bar_heights[i],
                                COLOR_PANEL);
    }
    s_track_hint = label(s_track_group, "走几步，看箭头和提示音变化。",
                         &finder_font_14, COLOR_MUTED,
                         LV_ALIGN_TOP_MID, 0, 232);
    label(s_track_group, "上下调音量 · OK 返回 · 长按设置",
          &finder_font_14, COLOR_MUTED, LV_ALIGN_TOP_MID, 0, 262);
}

static void make_settings_screen(void)
{
    static const char *names[] = {"提示音", "音量", "名称匹配", "滤波档位"};

    s_settings_group = make_group();
    label(s_settings_group, "设置", &finder_font_20, COLOR_CYAN,
          LV_ALIGN_TOP_LEFT, 12, 54);
    for (uint8_t i = 0U; i < FINDER_SETTING_COUNT; ++i) {
        int y = 100 + (int)i * 40;

        s_setting_rows[i] = block(s_settings_group, 12, y, 216, 34, COLOR_PANEL);
        label(s_setting_rows[i], names[i], &finder_font_14, COLOR_INK,
              LV_ALIGN_LEFT_MID, 10, 0);
        s_setting_values[i] = label(s_setting_rows[i], "", &finder_font_14,
                                    COLOR_CYAN, LV_ALIGN_RIGHT_MID, -10, 0);
    }
    s_setting_hint = label(s_settings_group, "上下选择 · OK 修改 · 长按返回",
                           &finder_font_14, COLOR_MUTED,
                           LV_ALIGN_TOP_MID, 0, 270);
}

void finder_ui_init(void)
{
    if (s_initialized) {
        return;
    }
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    header();
    make_scan_screen();
    make_track_screen();
    make_settings_screen();
    lv_scr_load(s_screen);
    s_initialized = true;
}

static void hide_all(void)
{
    lv_obj_add_flag(s_scan_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_track_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_settings_group, LV_OBJ_FLAG_HIDDEN);
}

static uint32_t level_color(finder_level_t level)
{
    switch (level) {
    case FINDER_LEVEL_VERY_CLOSE: return COLOR_RED;
    case FINDER_LEVEL_CLOSE: return COLOR_ORANGE;
    case FINDER_LEVEL_NEAR: return COLOR_YELLOW;
    case FINDER_LEVEL_FAR: return COLOR_CYAN;
    case FINDER_LEVEL_OUT:
    default: return COLOR_MUTED;
    }
}

static const char *level_name(finder_level_t level)
{
    switch (level) {
    case FINDER_LEVEL_VERY_CLOSE: return "就在附近";
    case FINDER_LEVEL_CLOSE: return "很近";
    case FINDER_LEVEL_NEAR: return "接近";
    case FINDER_LEVEL_FAR: return "尚远";
    case FINDER_LEVEL_OUT:
    default: return "未发现";
    }
}

static const char *trend_name(finder_trend_t trend, int8_t delta)
{
    static char text[32];

    if (trend == FINDER_TREND_CLOSER) {
        (void)snprintf(text, sizeof(text), "↑ 靠近  +%d dB", delta > 0 ? delta : 4);
    } else if (trend == FINDER_TREND_AWAY) {
        (void)snprintf(text, sizeof(text), "↓ 远离  %d dB", delta < 0 ? delta : -4);
    } else {
        (void)snprintf(text, sizeof(text), "→ 信号稳定");
    }
    return text;
}

static void refresh_scan(const finder_ui_model_t *model)
{
    char text[64];

    (void)snprintf(text, sizeof(text), "发现 %u 个设备 · 上下选择",
                   (unsigned)model->view_count);
    lv_label_set_text(s_scan_hint, text);
    for (uint8_t row = 0U; row < FINDER_LIST_ROWS; ++row) {
        uint8_t index = model->scroll_offset + row;

        if (index >= model->view_count) {
            lv_obj_add_flag(s_scan_rows[row], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_scan_rows[row], LV_OBJ_FLAG_HIDDEN);
        const finder_device_view_t *device = &model->devices[index];
        if (device->label[0] != '\0') {
            lv_label_set_text(s_scan_names[row], device->label);
        } else {
            (void)snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                           device->addr[0], device->addr[1], device->addr[2],
                           device->addr[3], device->addr[4], device->addr[5]);
            lv_label_set_text(s_scan_names[row], text);
        }
        (void)snprintf(text, sizeof(text), "%d", device->filtered_rssi);
        lv_label_set_text(s_scan_rssi[row], text);
        lv_obj_set_width(s_scan_bars[row],
                         device->filtered_rssi > -50 ? 80 :
                         device->filtered_rssi > -65 ? 58 :
                         device->filtered_rssi > -78 ? 34 : 12);
        lv_obj_set_style_bg_color(s_scan_bars[row],
                                  lv_color_hex(index == model->cursor
                                               ? COLOR_YELLOW : COLOR_CYAN), 0);
        lv_obj_set_style_bg_color(s_scan_rows[row],
                                  lv_color_hex(index == model->cursor
                                               ? 0x1E3150 : COLOR_PANEL), 0);
    }
}

static void refresh_track(const finder_ui_model_t *model)
{
    char text[80];
    uint32_t color = level_color(model->level);

    if (model->target_valid && model->target.label[0] != '\0') {
        lv_label_set_text(s_track_title, model->target.label);
    } else if (model->target_valid && model->target.has_name) {
        lv_label_set_text(s_track_title, model->target.name);
    } else if (model->target_valid) {
        (void)snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                       model->target.addr[0], model->target.addr[1],
                       model->target.addr[2], model->target.addr[3],
                       model->target.addr[4], model->target.addr[5]);
        lv_label_set_text(s_track_title, text);
    } else {
        lv_label_set_text(s_track_title, "未锁定目标");
    }
    if (model->target_valid && model->target_acquiring) {
        (void)snprintf(text, sizeof(text), "正在锁定，等待目标广播…");
    } else if (model->target_valid && model->target_present &&
               model->last_seen_age_ms > 2500U) {
        (void)snprintf(text, sizeof(text), "信号不稳，继续走 · %u ms 前有信号",
                       (unsigned)model->last_seen_age_ms);
    } else if (model->target_valid && model->target_present) {
        (void)snprintf(text, sizeof(text), "信号更新 %u ms 前 · 目标在线",
                       (unsigned)model->last_seen_age_ms);
    } else {
        (void)snprintf(text, sizeof(text), "目标不在广播中，走几步再试");
    }
    lv_label_set_text(s_track_subtitle, text);
    if (model->target_rssi == INT8_MIN) {
        lv_label_set_text(s_track_rssi, "--");
    } else {
        (void)snprintf(text, sizeof(text), "%d", model->target_rssi);
        lv_label_set_text(s_track_rssi, text);
    }
    lv_obj_set_style_text_color(s_track_rssi, lv_color_hex(color), 0);
    lv_label_set_text(s_track_level, level_name(model->level));
    lv_obj_set_style_text_color(s_track_level, lv_color_hex(color), 0);
    lv_label_set_text(s_track_trend, trend_name(model->trend, model->trend_delta_db));
    for (uint8_t i = 0U; i < FINDER_LEVEL_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_track_bars[i],
                                  lv_color_hex(i <= (uint8_t)model->level ? color
                                                                         : COLOR_PANEL), 0);
    }
    switch (model->level) {
    case FINDER_LEVEL_VERY_CLOSE:
        lv_label_set_text(s_track_hint, "很近了！原地转一圈，看信号在哪边最强。");
        break;
    case FINDER_LEVEL_CLOSE:
        lv_label_set_text(s_track_hint, "放慢脚步，短距离移动确认方向。");
        break;
    case FINDER_LEVEL_NEAR:
        lv_label_set_text(s_track_hint, "继续朝箭头方向走，提示音会变密。");
        break;
    case FINDER_LEVEL_FAR:
        lv_label_set_text(s_track_hint, "大范围走动，找到信号变强的方向。");
        break;
    default:
        lv_label_set_text(s_track_hint, "等待广播：让耳机开机，或点亮手机屏幕。");
        break;
    }
}

static void refresh_settings(const finder_ui_model_t *model)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%s", model->prefs.sound_enabled ? "开" : "关");
    lv_label_set_text(s_setting_values[FINDER_SETTING_SOUND], text);
    (void)snprintf(text, sizeof(text), "%u%%", (unsigned)model->prefs.volume);
    lv_label_set_text(s_setting_values[FINDER_SETTING_VOLUME], text);
    (void)snprintf(text, sizeof(text), "%s", model->prefs.match_by_name ? "开" : "关");
    lv_label_set_text(s_setting_values[FINDER_SETTING_MATCH_NAME], text);
    static const char *profiles[] = {"快速", "标准", "稳定"};
    uint8_t profile = (uint8_t)model->prefs.profile;
    lv_label_set_text(s_setting_values[FINDER_SETTING_PROFILE],
                      profiles[profile < 3U ? profile : 1U]);
    for (uint8_t i = 0U; i < FINDER_SETTING_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_setting_rows[i],
                                  lv_color_hex(i == (uint8_t)model->setting_focus
                                               ? 0x1E3150 : COLOR_PANEL), 0);
    }
    (void)s_setting_hint;
}

void finder_ui_render(const finder_ui_model_t *model)
{
    char text[16];

    if (!s_initialized || model == NULL) {
        return;
    }
    hide_all();
    if (model->battery < 0 || model->battery > 100) {
        lv_label_set_text(s_battery_label, "--");
    } else {
        (void)snprintf(text, sizeof(text), "%d%%", (int)model->battery);
        lv_label_set_text(s_battery_label, text);
    }
    if (model->state == FINDER_STATE_SCAN) {
        lv_obj_clear_flag(s_scan_group, LV_OBJ_FLAG_HIDDEN);
        refresh_scan(model);
    } else if (model->state == FINDER_STATE_TRACK) {
        lv_obj_clear_flag(s_track_group, LV_OBJ_FLAG_HIDDEN);
        refresh_track(model);
    } else {
        lv_obj_clear_flag(s_settings_group, LV_OBJ_FLAG_HIDDEN);
        refresh_settings(model);
    }
}
