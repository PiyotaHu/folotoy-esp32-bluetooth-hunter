#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_types.h"

/* Shared BLE advertising report. Kept ESP-IDF-free so host tests and the
 * simulator can compile against the exact firmware data structure. */
typedef struct {
    uint8_t addr[FINDER_ADDR_LEN];
    uint8_t addr_type;
    char name[FINDER_NAME_LEN];
    bool has_name;
    int8_t rssi;
    uint32_t timestamp_ms;
} ble_scan_report_t;
