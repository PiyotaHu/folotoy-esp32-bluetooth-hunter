#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_label.h"

static ble_scan_report_t report_for(const uint8_t addr[6])
{
    ble_scan_report_t report;

    memset(&report, 0, sizeof(report));
    memcpy(report.addr, addr, sizeof(report.addr));
    report.addr_type = 1U;
    report.rssi = -60;
    report.timestamp_ms = 1000U;
    return report;
}

static void apply(const uint8_t *advertising, size_t length,
                   ble_scan_report_t *report)
{
    device_label_info_t info;

    device_label_info_init(&info);
    device_label_parse_ad(advertising, length, &info);
    device_label_apply(&info, report);
}

int main(void)
{
    static const uint8_t addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    /* Complete Local Name "AirPods Pro" + Apple manufacturer data. */
    static const uint8_t named[] = {
        0x0C, 0x09, 'A', 'i', 'r', 'P', 'o', 'd', 's', ' ', 'P', 'r', 'o',
        0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19
    };
    /* Shortened name then a complete name in a later scan response. */
    static const uint8_t shortened[] = {0x05, 0x08, 'A', 'i', 'r', 'P'};
    static const uint8_t complete[] = {
        0x0C, 0x09, 'A', 'i', 'r', 'P', 'o', 'd', 's', ' ', 'P', 'r', 'o'
    };
    /* Anonymous earbuds: Apple proximity pairing, no Local Name at all. */
    static const uint8_t apple_buds[] = {0x04, 0xFF, 0x4C, 0x00, 0x07};
    /* AirTag / Find My network advertisement. */
    static const uint8_t airtag[] = {0x04, 0xFF, 0x4C, 0x00, 0x12};
    /* Truncated AD structure: length claims 10 bytes but only 2 follow. */
    static const uint8_t malformed[] = {0x0A, 0x09, 'X'};
    /* Appearance: Generic Phone (0x0040). */
    static const uint8_t phone[] = {0x03, 0x19, 0x40, 0x00};
    /* Google Fast Pair service UUID, advertised as 16-bit little endian. */
    static const uint8_t fast_pair[] = {0x03, 0x03, 0x2C, 0xFE};
    ble_scan_report_t report;
    device_label_info_t info;

    report = report_for(addr);
    apply(named, sizeof(named), &report);
    assert(report.has_name);
    assert(strcmp(report.name, "AirPods Pro") == 0);
    assert(strcmp(report.label, "AirPods Pro") == 0);
    assert(!report.label_inferred);
    assert(report.kind == FINDER_KIND_EARBUDS);

    /* A complete name from a scan response replaces a shortened one. */
    device_label_info_init(&info);
    device_label_parse_ad(shortened, sizeof(shortened), &info);
    device_label_parse_ad(complete, sizeof(complete), &info);
    assert(info.has_name && info.name_complete);
    assert(strcmp(info.name, "AirPods Pro") == 0);

    report = report_for(addr);
    apply(apple_buds, sizeof(apple_buds), &report);
    assert(!report.has_name);
    assert(report.label_inferred);
    assert(report.kind == FINDER_KIND_EARBUDS);
    assert(strcmp(report.label, "Earbuds 55:66") == 0);

    report = report_for(addr);
    apply(airtag, sizeof(airtag), &report);
    assert(report.kind == FINDER_KIND_TAG);
    assert(strcmp(report.label, "Tag 55:66") == 0);

    /* Malformed data must still produce a usable, labelled report. */
    report = report_for(addr);
    apply(malformed, sizeof(malformed), &report);
    assert(report.label[0] != '\0');
    assert(report.rssi == -60);

    report = report_for(addr);
    apply(phone, sizeof(phone), &report);
    assert(report.kind == FINDER_KIND_PHONE);

    report = report_for(addr);
    apply(fast_pair, sizeof(fast_pair), &report);
    assert(report.kind == FINDER_KIND_EARBUDS);

    /* Empty / null payloads are safe and still yield a label. */
    report = report_for(addr);
    apply(NULL, 0U, &report);
    assert(strcmp(report.label, "BLE 55:66") == 0);

    puts("test_device_label: PASS");
    return 0;
}
