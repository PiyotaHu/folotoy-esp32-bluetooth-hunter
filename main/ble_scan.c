#include "ble_scan.h"

#include <stdbool.h>
#include <string.h>

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

static const char *TAG = "ble_scan";
static ble_scan_callback_t s_callback;
static void *s_context;
static uint8_t s_own_addr_type;
static bool s_initialized;
static bool s_synced;
static volatile uint32_t s_dropped;

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

static uint8_t sanitize_char(uint8_t c)
{
    if (c >= 0x20U && c < 0x7fU) {
        return c;
    }
    return '?';
}

static void process_discovery(const struct ble_gap_disc_desc *discovery)
{
    struct ble_hs_adv_fields fields;
    ble_scan_report_t report;
    size_t name_length;

    if (discovery == NULL || s_callback == NULL) {
        return;
    }
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, discovery->data,
                                discovery->length_data) != 0) {
        return;
    }
    memset(&report, 0, sizeof(report));
    memcpy(report.addr, discovery->addr.val, sizeof(report.addr));
    report.addr_type = discovery->addr.type;
    report.rssi = discovery->rssi;
    report.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (fields.name != NULL && fields.name_len > 0U) {
        name_length = fields.name_len;
        if (name_length >= FINDER_NAME_LEN) {
            name_length = FINDER_NAME_LEN - 1U;
        }
        for (size_t i = 0U; i < name_length; ++i) {
            report.name[i] = (char)sanitize_char(fields.name[i]);
        }
        report.name[name_length] = '\0';
        report.has_name = true;
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
