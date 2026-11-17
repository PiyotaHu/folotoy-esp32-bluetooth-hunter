#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ble_scan.h"
#include "finder_audio.h"
#include "finder_audio_synth.h"
#include "finder_model.h"
#include "finder_storage.h"
#include "finder_ui.h"
#include "key_guard.h"

#define EVENT_QUEUE_LENGTH 32U
#define WORKER_TICK_MS 40U
#define BATTERY_POLL_MS 30000U
#define UI_MIN_INTERVAL_MS 80U
#define SAVE_DEBOUNCE_MS 2000U

typedef enum {
    EVENT_BLE_REPORT = 0,
    EVENT_BUTTON,
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        ble_scan_report_t report;
        struct {
            bsp_btn_t button;
            bsp_btn_ev_t event;
        } button;
    } data;
} app_event_t;

static const char *TAG = "ble_finder";
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)
static key_guard_t s_key_guard;
static bool s_input_ready;
static QueueHandle_t s_events;
static finder_model_t s_model;
static bool s_audio_ready;
static bool s_render_valid;
static uint32_t s_last_render_ms;
static uint32_t s_last_battery_ms;
static uint32_t s_last_save_request_ms;
static int16_t s_battery = -1;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void on_ble_report(const ble_scan_report_t *report, void *context)
{
    app_event_t event;
    QueueHandle_t queue = context;

    if (report == NULL || queue == NULL) {
        return;
    }
    event.type = EVENT_BLE_REPORT;
    event.data.report = *report;
    if (xQueueSend(queue, &event, 0U) != pdTRUE) {
        ble_scan_note_drop();
    }
}

static void on_key(bsp_btn_t button, bsp_btn_ev_t button_event, void *context)
{
    app_event_t event;
    QueueHandle_t queue = context;
    const bool was_armed = s_key_guard.armed;

    /* Consume the wake-key guard before the readiness check. A release can
     * arrive after the post-init voltage sample but before dispatch is ready;
     * dropping it here would leave the guard armed for the next real press. */
    if (key_guard_consume(&s_key_guard,
                          button_event == BSP_BTN_RELEASE,
                          esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "wake key released; input restored");
        }
        return;
    }
    if (!s_input_ready || queue == NULL) {
        return;
    }
    if (button_event == BSP_BTN_CLICK) {
        event.type = EVENT_BUTTON;
        event.data.button.button = button;
        event.data.button.event = button_event;
        if (xQueueSend(queue, &event, 0U) != pdTRUE) {
            ESP_LOGW(TAG, "button queue full");
        }
    } else if (button_event == BSP_BTN_LONG && button == BSP_BTN_OK) {
        event.type = EVENT_BUTTON;
        event.data.button.button = button;
        event.data.button.event = button_event;
        if (xQueueSend(queue, &event, 0U) != pdTRUE) {
            ESP_LOGW(TAG, "button queue full");
        }
    }
}

static finder_button_t map_button(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event == BSP_BTN_LONG) {
        return FINDER_BTN_OK_LONG;
    }
    if (button == BSP_BTN_UP) {
        return FINDER_BTN_UP;
    }
    if (button == BSP_BTN_DOWN) {
        return FINDER_BTN_DOWN;
    }
    return FINDER_BTN_OK;
}

static void play(finder_cue_t cue, finder_level_t level)
{
    if (s_audio_ready) {
        (void)finder_audio_play(cue, level);
    }
}

static void dispatch_actions(finder_actions_t actions, uint32_t timestamp_ms)
{
    finder_level_t level = s_model.reported_level;

    if ((actions & FINDER_ACTION_CUE_LOCK) != 0U &&
        s_model.prefs.sound_enabled) {
        play(FINDER_CUE_LOCK, level);
    }
    if ((actions & FINDER_ACTION_CUE_LEVEL) != 0U) {
        if (s_model.prefs.sound_enabled) {
            play(FINDER_CUE_LEVEL, level);
        }
    }
    if ((actions & FINDER_ACTION_CUE_CLOSER) != 0U) {
        if (s_model.prefs.sound_enabled) {
            play(FINDER_CUE_CLOSER, level);
        }
    }
    if ((actions & FINDER_ACTION_CUE_AWAY) != 0U) {
        if (s_model.prefs.sound_enabled) {
            play(FINDER_CUE_AWAY, level);
        }
    }
    if ((actions & FINDER_ACTION_CUE_FOUND) != 0U) {
        if (s_model.prefs.sound_enabled) {
            play(FINDER_CUE_FOUND, FINDER_LEVEL_VERY_CLOSE);
        }
    }
    if ((actions & FINDER_ACTION_SAVE) != 0U) {
        s_last_save_request_ms = timestamp_ms;
    }
}

static void save_if_due(uint32_t timestamp_ms, bool force)
{
    finder_target_t target;
    esp_err_t err;

    if (!force &&
        (s_last_save_request_ms == 0U ||
         (uint32_t)(timestamp_ms - s_last_save_request_ms) < SAVE_DEBOUNCE_MS)) {
        return;
    }
    target = finder_model_target(&s_model);
    err = finder_storage_save(&s_model.prefs, &target, s_model.target_locked);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
    }
    s_last_save_request_ms = 0U;
}

static void poll_battery(uint32_t timestamp_ms, bool force)
{
    int value;

    if (!force && (uint32_t)(timestamp_ms - s_last_battery_ms) < BATTERY_POLL_MS) {
        return;
    }
    s_last_battery_ms = timestamp_ms;
    value = bsp_battery_soc();
    if (value >= 0 && value <= 100) {
        s_battery = (int16_t)value;
    } else {
        s_battery = -1;
    }
}

static void render(uint32_t timestamp_ms, bool force)
{
    finder_ui_model_t view;

    if (!force && (uint32_t)(timestamp_ms - s_last_render_ms) < UI_MIN_INTERVAL_MS) {
        return;
    }
    finder_model_view(&s_model, &view, timestamp_ms);
    view.battery = s_battery;
    if (!bsp_lvgl_lock(200)) {
        ESP_LOGW(TAG, "LVGL lock timeout");
        return;
    }
    finder_ui_render(&view);
    bsp_lvgl_unlock();
    s_last_render_ms = timestamp_ms;
    s_render_valid = true;
}

void app_main(void)
{
    finder_preferences_t prefs;
    finder_target_t target;
    bool has_target = false;
    app_event_t event;
    esp_err_t err;
    uint32_t timestamp_ms;
    uint32_t wake_causes;

    ESP_LOGI(TAG, "starting BLE signal finder");
    wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1U << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "wake key guard armed");
    }
    err = finder_storage_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }
    err = finder_storage_load(&prefs, &target, &has_target);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "load failed: %s", esp_err_to_name(err));
    }
    err = bsp_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed: %s", esp_err_to_name(err));
    }
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "display init failed");
        return;
    }
    bsp_display_backlight(100U);
    if (bsp_lvgl_lock(1000)) {
        finder_ui_init();
        bsp_lvgl_unlock();
    }
    poll_battery(0, true);
    s_audio_ready = finder_audio_init() == ESP_OK;
    if (!s_audio_ready) {
        ESP_LOGW(TAG, "audio unavailable; continuing in visual mode");
    } else {
        (void)finder_audio_set_volume(prefs.volume);
    }
    timestamp_ms = now_ms();
    finder_model_init(&s_model, &prefs, &target, has_target, timestamp_ms);

    s_events = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(app_event_t));
    if (s_events == NULL) {
        ESP_LOGE(TAG, "event queue allocation failed");
        return;
    }
    if (bsp_button_init(on_key, s_events) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
    } else if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) {
            ESP_LOGI(TAG, "wake key already released; guard disarmed");
        }
    }
    s_input_ready = true;
    err = ble_scan_init(on_ble_report, s_events);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(err));
    }
    render(now_ms(), true);

    while (true) {
        finder_actions_t actions = FINDER_ACTION_NONE;
        uint32_t wait_ms = WORKER_TICK_MS;

        while (xQueueReceive(s_events, &event, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
            wait_ms = 0U;
            timestamp_ms = now_ms();
            if (event.type == EVENT_BLE_REPORT) {
                actions |= finder_model_on_report(&s_model, &event.data.report,
                                                  timestamp_ms);
            } else {
                finder_button_t button = map_button(event.data.button.button,
                                                    event.data.button.event);
                actions |= finder_model_on_button(&s_model, button, timestamp_ms);
                play(FINDER_CUE_SETTING, s_model.reported_level);
            }
        }
        timestamp_ms = now_ms();
        actions |= finder_model_tick(&s_model, timestamp_ms);
        poll_battery(timestamp_ms, false);
        dispatch_actions(actions, timestamp_ms);
        if ((actions & (FINDER_ACTION_UI_CHANGED | FINDER_ACTION_CUE_LEVEL)) != 0U ||
            !s_render_valid) {
            render(timestamp_ms, false);
        }
        save_if_due(timestamp_ms, false);
    }
}
