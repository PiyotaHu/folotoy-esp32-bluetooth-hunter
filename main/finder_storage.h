#pragma once

#include "esp_err.h"
#include "finder_types.h"

esp_err_t finder_storage_init(void);
esp_err_t finder_storage_load(finder_preferences_t *prefs,
                              finder_target_t *target,
                              bool *has_target);
esp_err_t finder_storage_save(const finder_preferences_t *prefs,
                              const finder_target_t *target,
                              bool has_target);
