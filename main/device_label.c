#include "device_label.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Bluetooth AD structure types (Core Specification, CSS v11 Part A). */
enum {
    AD_FLAGS = 0x01,
    AD_UUID16_SOME = 0x02,
    AD_UUID16_ALL = 0x03,
    AD_UUID32_SOME = 0x04,
    AD_UUID32_ALL = 0x05,
    AD_UUID128_SOME = 0x06,
    AD_UUID128_ALL = 0x07,
    AD_NAME_SHORT = 0x08,
    AD_NAME_COMPLETE = 0x09,
    AD_TX_POWER = 0x0A,
    AD_SERVICE_DATA_16 = 0x16,
    AD_APPEARANCE = 0x19,
    AD_MANUFACTURER = 0xFF,
};

/* Company Identifier Codes (Bluetooth Assigned Numbers). */
enum {
    COMPANY_MICROSOFT = 0x0006,
    COMPANY_APPLE = 0x004C,
    COMPANY_NORDIC = 0x0059,
    COMPANY_SAMSUNG = 0x0075,
    COMPANY_GOOGLE = 0x00E0,
    COMPANY_HUAWEI = 0x027D,
    COMPANY_ESPRESSIF = 0x02E5,
    COMPANY_XIAOMI = 0x038F,
};

/* Apple manufacturer-data subtypes we can reasonably name. */
enum {
    APPLE_IBEACON = 0x02,
    APPLE_AIRDROP = 0x05,
    APPLE_PROXIMITY_PAIRING = 0x07,
    APPLE_NEARBY_INFO = 0x10,
    APPLE_FIND_MY = 0x12,
};

/* 16-bit Service UUIDs that hint at a device class. */
enum {
    UUID_HANDSFREE = 0x111E,
    UUID_HEADSET = 0x1108,
    UUID_AUDIO_SINK = 0x110B,
    UUID_AUDIO_SOURCE = 0x110A,
    UUID_HEART_RATE = 0x180D,
    UUID_HID = 0x1812,
    UUID_GOOGLE_FAST_PAIR = 0xFE2C,
};

static const char *kind_text(finder_device_kind_t kind)
{
    switch (kind) {
    case FINDER_KIND_EARBUDS: return "Earbuds";
    case FINDER_KIND_PHONE: return "Phone";
    case FINDER_KIND_WATCH: return "Watch";
    case FINDER_KIND_TAG: return "Tag";
    case FINDER_KIND_COMPUTER: return "Computer";
    case FINDER_KIND_SPEAKER: return "Speaker";
    case FINDER_KIND_BEACON: return "Beacon";
    case FINDER_KIND_OTHER: return "Device";
    case FINDER_KIND_UNKNOWN:
    default: return "BLE";
    }
}

void device_label_info_init(device_label_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->company_id = DEVICE_LABEL_COMPANY_NONE;
    info->appearance = DEVICE_LABEL_APPEARANCE_NONE;
    info->kind = FINDER_KIND_UNKNOWN;
}

static void set_name(device_label_info_t *info, const uint8_t *value,
                     size_t length, bool complete)
{
    char cleaned[FINDER_NAME_LEN];
    size_t out = 0U;
    bool non_ascii = false;

    /* The bundled CJK fonts are deliberately small subsets. Advertising names
     * outside ASCII would render as placeholder boxes, so drop those bytes and
     * fall back to an inferred ASCII label when nothing usable remains. */
    for (size_t i = 0U; i < length && out + 1U < sizeof(cleaned); ++i) {
        uint8_t c = value[i];

        if (c >= 0x80U) {
            non_ascii = true;
            continue;
        }
        if (c == 0U) {
            break;
        }
        cleaned[out++] = (char)(isprint((unsigned char)c) ? c : '?');
    }
    cleaned[out] = '\0';
    if (out == 0U) {
        return;
    }
    /* A complete name always wins; otherwise keep the longest variant. */
    if (info->has_name &&
        ((info->name_complete && !complete) || out <= info->name_length)) {
        return;
    }
    memcpy(info->name, cleaned, out + 1U);
    info->name_length = out;
    info->name_complete = complete;
    info->has_name = true;
    (void)non_ascii;
}

static void add_uuid(device_label_info_t *info, uint16_t uuid)
{
    if (info->uuid_count >= DEVICE_LABEL_MAX_UUIDS) {
        return;
    }
    for (uint8_t i = 0U; i < info->uuid_count; ++i) {
        if (info->uuids[i] == uuid) {
            return;
        }
    }
    info->uuids[info->uuid_count++] = uuid;
}

void device_label_parse_ad(const uint8_t *data, size_t length,
                           device_label_info_t *info)
{
    size_t position = 0U;

    if (data == NULL || info == NULL) {
        return;
    }
    while (position + 2U <= length) {
        size_t ad_length = data[position];

        if (ad_length == 0U) {
            break; /* zero padding, not a malformed structure */
        }
        if (position + 1U + ad_length > length) {
            break; /* truncated: keep everything decoded so far */
        }
        const uint8_t type = data[position + 1U];
        const uint8_t *value = &data[position + 2U];
        const size_t value_length = ad_length - 1U;

        switch (type) {
        case AD_NAME_SHORT:
        case AD_NAME_COMPLETE:
            set_name(info, value, value_length, type == AD_NAME_COMPLETE);
            break;
        case AD_APPEARANCE:
            if (value_length >= 2U) {
                info->appearance = (uint16_t)((uint16_t)value[0] |
                                              ((uint16_t)value[1] << 8));
            }
            break;
        case AD_MANUFACTURER:
            if (value_length >= 2U) {
                info->company_id = (uint16_t)((uint16_t)value[0] |
                                              ((uint16_t)value[1] << 8));
                if (value_length >= 3U) {
                    info->manufacturer_type = value[2];
                }
            }
            break;
        case AD_UUID16_SOME:
        case AD_UUID16_ALL:
        case AD_SERVICE_DATA_16:
            for (size_t i = 0U; i + 1U < value_length; i += 2U) {
                add_uuid(info, (uint16_t)((uint16_t)value[i] |
                                          ((uint16_t)value[i + 1U] << 8)));
            }
            break;
        case AD_UUID128_SOME:
        case AD_UUID128_ALL:
            /* Google Fast Pair and similar 128-bit services are intentionally
             * not decoded here; vendor data already covers those cases. */
            break;
        default:
            break;
        }
        position += 1U + ad_length;
    }
}

static finder_device_kind_t kind_from_name(const char *name)
{
    /* Case-insensitive comparison without locale-dependent toupper(). */
    static const struct { const char *needle; finder_device_kind_t kind; } rules[] = {
        {"airpod", FINDER_KIND_EARBUDS}, {"buds", FINDER_KIND_EARBUDS},
        {"earbud", FINDER_KIND_EARBUDS}, {"headset", FINDER_KIND_EARBUDS},
        {"headphone", FINDER_KIND_EARBUDS}, {"wh-1000", FINDER_KIND_EARBUDS},
        {"airtag", FINDER_KIND_TAG}, {"tile", FINDER_KIND_TAG},
        {"tracker", FINDER_KIND_TAG}, {"smarttag", FINDER_KIND_TAG},
        {"watch", FINDER_KIND_WATCH}, {"band", FINDER_KIND_WATCH},
        {"iphone", FINDER_KIND_PHONE}, {"phone", FINDER_KIND_PHONE},
        {"galaxy s", FINDER_KIND_PHONE}, {"pixel", FINDER_KIND_PHONE},
        {"macbook", FINDER_KIND_COMPUTER}, {"laptop", FINDER_KIND_COMPUTER},
        {"speaker", FINDER_KIND_SPEAKER}, {"jbl", FINDER_KIND_SPEAKER},
        {"beacon", FINDER_KIND_BEACON},
    };
    char lowered[FINDER_NAME_LEN];
    size_t i = 0U;

    for (; name[i] != '\0' && i + 1U < sizeof(lowered); ++i) {
        lowered[i] = (char)tolower((unsigned char)name[i]);
    }
    lowered[i] = '\0';
    for (i = 0U; i < sizeof(rules) / sizeof(rules[0]); ++i) {
        if (strstr(lowered, rules[i].needle) != NULL) {
            return rules[i].kind;
        }
    }
    return FINDER_KIND_UNKNOWN;
}

static finder_device_kind_t kind_from_appearance(uint16_t appearance)
{
    /* Appearance is category << 6 | subcategory. */
    switch (appearance >> 6) {
    case 0x01U: return FINDER_KIND_PHONE;    /* 0x0040 Generic Phone */
    case 0x02U: return FINDER_KIND_COMPUTER; /* 0x0080 Generic Computer */
    case 0x03U:                              /* 0x00C0 Generic Watch */
    case 0x0DU: return FINDER_KIND_WATCH;    /* 0x0340 Heart Rate Sensor */
    case 0x08U: return FINDER_KIND_TAG;      /* 0x0200 Generic Tag */
    case 0x0AU: return FINDER_KIND_SPEAKER;  /* 0x0280 Media Player */
    case 0x0FU: return FINDER_KIND_COMPUTER; /* 0x03C0 HID */
    case 0x25U: return FINDER_KIND_EARBUDS;  /* 0x0940 Wearable Audio */
    default: return FINDER_KIND_UNKNOWN;
    }
}

static finder_device_kind_t kind_from_uuids(const device_label_info_t *info)
{
    for (uint8_t i = 0U; i < info->uuid_count; ++i) {
        switch (info->uuids[i]) {
        case UUID_GOOGLE_FAST_PAIR:
        case UUID_HEADSET:
        case UUID_HANDSFREE:
        case UUID_AUDIO_SINK:
        case UUID_AUDIO_SOURCE:
            return FINDER_KIND_EARBUDS;
        case UUID_HEART_RATE:
            return FINDER_KIND_WATCH;
        case UUID_HID:
            return FINDER_KIND_COMPUTER;
        default:
            break;
        }
    }
    return FINDER_KIND_UNKNOWN;
}

static const char *vendor_text(uint16_t company_id)
{
    switch (company_id) {
    case COMPANY_APPLE: return "Apple";
    case COMPANY_SAMSUNG: return "Samsung";
    case COMPANY_GOOGLE: return "Google";
    case COMPANY_HUAWEI: return "Huawei";
    case COMPANY_XIAOMI: return "Xiaomi";
    case COMPANY_MICROSOFT: return "Microsoft";
    case COMPANY_ESPRESSIF: return "Espressif";
    case COMPANY_NORDIC: return "Nordic";
    default: return NULL;
    }
}

static finder_device_kind_t infer_kind(const device_label_info_t *info)
{
    if (info->has_name) {
        finder_device_kind_t from_name = kind_from_name(info->name);

        if (from_name != FINDER_KIND_UNKNOWN) {
            return from_name;
        }
    }
    if (info->company_id == COMPANY_APPLE) {
        switch (info->manufacturer_type) {
        case APPLE_IBEACON: return FINDER_KIND_BEACON;
        case APPLE_PROXIMITY_PAIRING: return FINDER_KIND_EARBUDS;
        case APPLE_FIND_MY: return FINDER_KIND_TAG;
        case APPLE_NEARBY_INFO:
        case APPLE_AIRDROP: return FINDER_KIND_PHONE;
        default: break;
        }
    }
    if (info->appearance != DEVICE_LABEL_APPEARANCE_NONE) {
        finder_device_kind_t from_appearance =
            kind_from_appearance(info->appearance);

        if (from_appearance != FINDER_KIND_UNKNOWN) {
            return from_appearance;
        }
    }
    {
        finder_device_kind_t from_uuids = kind_from_uuids(info);

        if (from_uuids != FINDER_KIND_UNKNOWN) {
            return from_uuids;
        }
    }
    if (info->company_id != DEVICE_LABEL_COMPANY_NONE) {
        return FINDER_KIND_OTHER;
    }
    return FINDER_KIND_UNKNOWN;
}

void device_label_apply(const device_label_info_t *info,
                        ble_scan_report_t *report)
{
    const char *vendor;
    const char *text;

    if (info == NULL || report == NULL) {
        return;
    }
    report->kind = infer_kind(info);
    if (info->has_name) {
        memcpy(report->name, info->name, sizeof(report->name));
        report->has_name = true;
    }
    if (report->has_name && report->name[0] != '\0') {
        /* label[] is shorter than name[]; truncate through snprintf so no
         * unterminated or out-of-bounds bytes can reach the LVGL label. */
        (void)snprintf(report->label, sizeof(report->label), "%s", report->name);
        report->label_inferred = false;
        return;
    }
    vendor = vendor_text(info->company_id);
    text = vendor != NULL && report->kind == FINDER_KIND_OTHER
               ? vendor : kind_text(report->kind);
    /* Short MAC tail keeps otherwise identical anonymous devices distinct. */
    (void)snprintf(report->label, sizeof(report->label), "%s %02X:%02X",
                   text, report->addr[4], report->addr[5]);
    report->label_inferred = true;
}
