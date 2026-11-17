#include "finder_storage.h"

#include <string.h>

#include "finder_model.h"
#include "nvs.h"
#include "nvs_flash.h"

#define FINDER_NVS_NAMESPACE "ble_finder"
#define FINDER_NVS_SOUND "sound"
#define FINDER_NVS_VOLUME "volume"
#define FINDER_NVS_MATCH "match"
#define FINDER_NVS_PROFILE "profile"
#define FINDER_NVS_TARGET "target"

#define FINDER_TARGET_MAGIC 0x42464631U // "BFF1"
#define FINDER_TARGET_VERSION 1U

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t addr_type;
    uint8_t has_name;
    uint8_t reserved;
    uint8_t addr[FINDER_ADDR_LEN];
    char name[FINDER_NAME_LEN];
} finder_target_record_t;

static esp_err_t read_optional_u8(nvs_handle_t handle, const char *key, uint8_t *value)
{
    esp_err_t err = nvs_get_u8(handle, key, value);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

esp_err_t finder_storage_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

esp_err_t finder_storage_load(finder_preferences_t *prefs,
                              finder_target_t *target,
                              bool *has_target)
{
    finder_preferences_t defaults;
    nvs_handle_t handle;
    finder_target_record_t record;
    size_t record_size = sizeof(record);
    uint8_t sound;
    uint8_t volume;
    uint8_t match;
    uint8_t profile;
    esp_err_t err;

    if (prefs == NULL || target == NULL || has_target == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    finder_preferences_defaults(&defaults);
    *prefs = defaults;
    memset(target, 0, sizeof(*target));
    *has_target = false;

    err = nvs_open(FINDER_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    sound = defaults.sound_enabled ? 1U : 0U;
    volume = defaults.volume;
    match = defaults.match_by_name ? 1U : 0U;
    profile = (uint8_t)defaults.profile;
    if (read_optional_u8(handle, FINDER_NVS_SOUND, &sound) == ESP_OK &&
        read_optional_u8(handle, FINDER_NVS_VOLUME, &volume) == ESP_OK &&
        read_optional_u8(handle, FINDER_NVS_MATCH, &match) == ESP_OK &&
        read_optional_u8(handle, FINDER_NVS_PROFILE, &profile) == ESP_OK) {
        prefs->sound_enabled = sound != 0U;
        prefs->volume = volume <= 100U ? volume : defaults.volume;
        prefs->match_by_name = match != 0U;
        if (profile < (uint8_t)FINDER_PROFILE_COUNT) {
            prefs->profile = (finder_profile_t)profile;
        }
    }
    err = nvs_get_blob(handle, FINDER_NVS_TARGET, &record, &record_size);
    nvs_close(handle);
    if (err == ESP_OK && record_size == sizeof(record) &&
        record.magic == FINDER_TARGET_MAGIC && record.version == FINDER_TARGET_VERSION) {
        memcpy(target->addr, record.addr, sizeof(target->addr));
        target->addr_type = record.addr_type;
        if (record.has_name != 0U) {
            memcpy(target->name, record.name, sizeof(target->name));
            target->name[FINDER_NAME_LEN - 1U] = '\0';
            target->has_name = true;
        }
        *has_target = true;
    }
    return ESP_OK;
}

esp_err_t finder_storage_save(const finder_preferences_t *prefs,
                              const finder_target_t *target,
                              bool has_target)
{
    nvs_handle_t handle;
    finder_target_record_t record;
    esp_err_t err;

    if (prefs == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    err = nvs_open(FINDER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(handle, FINDER_NVS_SOUND, prefs->sound_enabled ? 1U : 0U);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, FINDER_NVS_VOLUME, prefs->volume);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, FINDER_NVS_MATCH, prefs->match_by_name ? 1U : 0U);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, FINDER_NVS_PROFILE, (uint8_t)prefs->profile);
    }
    if (err == ESP_OK && has_target && target != NULL) {
        memset(&record, 0, sizeof(record));
        record.magic = FINDER_TARGET_MAGIC;
        record.version = FINDER_TARGET_VERSION;
        record.addr_type = target->addr_type;
        record.has_name = target->has_name ? 1U : 0U;
        memcpy(record.addr, target->addr, sizeof(record.addr));
        if (target->has_name) {
            memcpy(record.name, target->name, sizeof(record.name));
            record.name[FINDER_NAME_LEN - 1U] = '\0';
        }
        err = nvs_set_blob(handle, FINDER_NVS_TARGET, &record, sizeof(record));
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
