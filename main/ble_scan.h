#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "finder_report.h"
#include "finder_types.h"

typedef void (*ble_scan_callback_t)(const ble_scan_report_t *report, void *context);

esp_err_t ble_scan_init(ble_scan_callback_t callback, void *context);
uint32_t ble_scan_dropped_count(void);
void ble_scan_note_drop(void);
