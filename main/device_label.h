#pragma once

#include <stddef.h>
#include <stdint.h>

#include "finder_report.h"

/* Raw advertising / scan-response decoder. Deliberately free of ESP-IDF and
 * NimBLE headers so the exact shipping logic can be exercised by host tests.
 *
 * Why this exists: the previous code called ble_hs_adv_parse_fields() and
 * dropped the whole report when parsing failed. Real-world advertisements are
 * frequently truncated or malformed, and a scan response carries a different
 * set of AD structures than the primary advertisement. Dropping those reports
 * loses RSSI samples (=> "no signal") and loses the Local Name (=> only MAC).
 * This decoder is total: every byte sequence yields a usable report. */

#define DEVICE_LABEL_COMPANY_NONE 0xFFFFU
#define DEVICE_LABEL_APPEARANCE_NONE 0xFFFFU
#define DEVICE_LABEL_MAX_UUIDS 8U

typedef struct {
    char name[FINDER_NAME_LEN];
    bool has_name;
    bool name_complete;             /* 0x09 Complete Local Name wins over 0x08 */
    size_t name_length;
    finder_device_kind_t kind;
    uint16_t company_id;
    uint16_t appearance;
    uint16_t uuids[DEVICE_LABEL_MAX_UUIDS];
    uint8_t uuid_count;
    uint8_t manufacturer_type;      /* first payload byte of Apple 0xFF AD */
} device_label_info_t;

void device_label_info_init(device_label_info_t *info);

/* Walk AD structures. Malformed/truncated input stops the walk but never
 * invalidates fields already collected. */
void device_label_parse_ad(const uint8_t *data, size_t length,
                           device_label_info_t *info);

/* Infer a category from all collected hints, then fill report->label.
 * label is always ASCII so it renders with the bundled subset fonts. */
void device_label_apply(const device_label_info_t *info,
                        ble_scan_report_t *report);
