/*
 * Synthetic BLE report source for PassportSim QA.
 * This file is compiled only when CONFIG_FINDER_EMULATOR_BUILD=y.
 * It intentionally produces deterministic, rich reports so every UI state can
 * be exercised without a radio. It is NOT part of the release firmware.
 */
#include "ble_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SIM_TASK_STACK 3072U
#define SIM_TASK_PRIORITY 3U
#define SIM_PERIOD_MS 120U

static const char *TAG = "ble_scan_sim";
static ble_scan_callback_t s_callback;
static void *s_context;
static volatile uint32_t s_dropped;
static bool s_initialized;

static int8_t simulated_rssi(uint32_t tick)
{
    static const int8_t steps[] = {
        -88, -84, -81, -78, -75, -72, -70, -67, -64, -61, -58, -55,
        -52, -49, -46, -43, -40, -38, -40, -43, -46, -49, -52, -55,
        -58, -61, -64, -67, -70, -73, -76, -79, -82, -85, -88
    };

    return steps[tick % (sizeof(steps) / sizeof(steps[0]))];
}

static void emit(const uint8_t addr[FINDER_ADDR_LEN], uint8_t addr_type,
                 const char *name, int8_t rssi, uint32_t now_ms)
{
    ble_scan_report_t report;

    if (s_callback == NULL) {
        return;
    }
    memset(&report, 0, sizeof(report));
    memcpy(report.addr, addr, sizeof(report.addr));
    report.addr_type = addr_type;
    if (name != NULL) {
        size_t length = strlen(name);

        if (length >= FINDER_NAME_LEN) {
            length = FINDER_NAME_LEN - 1U;
        }
        memcpy(report.name, name, length);
        report.name[length] = '\0';
        report.has_name = true;
    }
    report.rssi = rssi;
    report.timestamp_ms = now_ms;
    s_callback(&report, s_context);
}

static void sim_task(void *argument)
{
    static const uint8_t earbuds[FINDER_ADDR_LEN] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };
    static const uint8_t phone[FINDER_ADDR_LEN] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66
    };
    static const uint8_t tag[FINDER_ADDR_LEN] = {
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F
    };
    uint32_t tick = 0U;

    (void)argument;
    while (true) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        int8_t rssi = simulated_rssi(tick);

        emit(earbuds, 1U, "AirPods Pro", rssi, now_ms);
        emit(phone, 1U, "iPhone", (int8_t)(-80 + (tick % 5U)), now_ms + 1U);
        emit(tag, 1U, "Tag", (int8_t)(-72 + (tick % 9U)), now_ms + 2U);
        ++tick;
        vTaskDelay(pdMS_TO_TICKS(SIM_PERIOD_MS));
    }
}

esp_err_t ble_scan_init(ble_scan_callback_t callback, void *context)
{
    if (callback == NULL || s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_callback = callback;
    s_context = context;
    s_initialized = true;
    if (xTaskCreate(sim_task, "ble_sim", SIM_TASK_STACK, NULL,
                    SIM_TASK_PRIORITY, NULL) != pdPASS) {
        s_initialized = false;
        s_callback = NULL;
        s_context = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGW(TAG, "simulator QA build: synthetic BLE reports enabled");
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
