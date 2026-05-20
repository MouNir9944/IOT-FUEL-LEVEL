/*
 * Copyright (c) 2024 <[SFM Technologies]>
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "nvs_flash.h"
#include "nvs.h"
#include "SIM7600.h"


char* get_saved_data_from_flash(const char* namespace, const char* key);
esp_err_t saved_data_in_flash(const char* namespace, const char* key, const char* value);

esp_err_t load_saved_gps_from_nvs();

#endif /*  NVS_MANAGER_H*/