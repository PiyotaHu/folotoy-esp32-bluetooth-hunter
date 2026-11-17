#include "ble_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "device_label.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#define BLE_SCAN_ITVL 160U   // 100 ms; dense enough for walking feedback
#define BLE_SCAN_WINDOW 160U // continuous duty cycle: freshest RSSI we can get

/* A busy room can produce hundreds of advertisements per second. Forwarding
 * every one of them overflows the event queue and silently drops the packets
 * that actually belong to the tracked device. Coalesce per address at ~10 Hz;
 * a name-bearing scan response always gets through. */
#define BLE_REPORT_MIN_INTERVAL_MS 90U
#define BLE_THROTTLE_SLOTS 24U

typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    uint32_t last_ms;
    bool had_name;
    bool used;
} throttle_entry_t;

static const char *TAG = "ble_scan";
static ble_scan_callback_t s_callback;
static void *s_context;
static uint8_t s_own_addr_type;
static bool s_initialized;
static bool s_synced;
static volatile uint32_t s_dropped;
static throttle_entry_t s_throttle[BLE_THROTTLE_SLOTS];
static uint8_t s_throttle_cursor;

static int gap_event(struct ble_gap_event *event, void *context);

static int start_scanning(void)
{
    struct ble_gap_disc_params params;

    if (ble_gap_disc_active()) {
        return 0;
    }
    memset(&params, 0, sizeof(params));
    params.itvl = BLE_SCAN_ITVL;
    params.window = BLE_SCAN_WINDOW;
    params.filter_duplicates = 0; // repeated reports drive fresh RSSI
    params.passive = 0;           // active scan also receives scan-response names
    return ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &params,
                        gap_event, NULL);
}

static bool should_forward(const ble_scan_report_t *report)
{
    throttle_entry_t *slot = NULL;
    uint32_t now = report->timestamp_ms;

    for (uint8_t i = 0U; i < BLE_THROTTLE_SLOTS; ++i) {
        if (s_throttle[i].used &&
            s_throttle[i].addr_type == report->addr_type &&
            memcmp(s_throttle[i].addr, report->addr, FINDER_ADDR_LEN) == 0) {
            slot = &s_throttle[i];
            break;
        }
    }
    if (slot == NULL) {
        slot = &s_throttle[s_throttle_cursor];
        s_throttle_cursor = (uint8_t)((s_throttle_cursor + 1U) %
                                     BLE_THROTTLE_SLOTS);
        memset(slot, 0, sizeof(*slot));
        memcpy(slot->addr, report->addr, sizeof(slot->addr));
        slot->addr_type = report->addr_type;
        slot->used = true;
        slot->last_ms = now;
        slot->had_name = report->has_name;
        return true;
    }
    /* The first name-bearing scan response must always get through, otherwise
     * an anonymous advertiser stays a bare MAC forever. Once we have seen the
     * name, ordinary packets are coalesced at ~10 Hz even if every single
     * advertisement carries the Local Name. */
    bool name_discovered = report->has_name && !slot->had_name;

    if (!name_discovered &&
        (uint32_t)(now - slot->last_ms) < BLE_REPORT_MIN_INTERVAL_MS) {
        return false;
    }
    slot->last_ms = now;
    slot->had_name = (bool)(slot->had_name || report->has_name);
    return true;
}

static void process_discovery(const struct ble_gap_disc_desc *discovery)
{
    device_label_info_t info;
    ble_scan_report_t report;

    if (discovery == NULL || s_callback == NULL) {
        return;
    }
    memset(&report, 0, sizeof(report));
    memcpy(report.addr, discovery->addr.val, sizeof(report.addr));
    report.addr_type = discovery->addr.type;
    report.rssi = discovery->rssi;
    report.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* The old path called ble_hs_adv_parse_fields() and returned on any
     * parse error, throwing away the RSSI too. This decoder is total: even
     * malformed or unknown advertising data still updates the signal. */
    device_label_info_init(&info);
    device_label_parse_ad(discovery->data, discovery->length_data, &info);
    device_label_apply(&info, &report);

    if (!should_forward(&report)) {
        return;
    }
    s_callback(&report, s_context);
}

static int gap_event(struct ble_gap_event *event, void *context)
{
    int rc;

    (void)context;
    if (event == NULL) {
        return 0;
    }
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        process_discovery(&event->disc);
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        rc = start_scanning();
        if (rc != 0) {
            ESP_LOGW(TAG, "scan restart failed: %d", rc);
        }
        break;
    default:
        break;
    }
    return 0;
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE reset: %d", reason);
    s_synced = false;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);

    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "BLE identity failed: %d", rc);
        return;
    }
    s_synced = true;
    rc = start_scanning();
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "observer scan started; no link, no pairing");
}

static void host_task(void *context)
{
    (void)context;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_scan_init(ble_scan_callback_t callback, void *context)
{
    int rc;

    if (callback == NULL || s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_callback = callback;
    s_context = context;
    rc = nimble_port_init();
    if (rc != 0) {
        s_callback = NULL;
        s_context = NULL;
        return rc == BLE_HS_ENOMEM ? ESP_ERR_NO_MEM : ESP_FAIL;
    }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    s_initialized = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

uint32_t ble_scan_dropped_count(void)
{
    return s_dropped;
}

void ble_scan_note_drop(void)
{
    ++s_dropped;
}
