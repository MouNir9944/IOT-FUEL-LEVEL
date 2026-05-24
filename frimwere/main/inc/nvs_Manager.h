/*
 * Copyright (c) 2024 <[SFM Technologies]>
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "nvs_flash.h"
#include "nvs.h"
#include "SIM7600.h"

/* ── String key/value helpers ─────────────────────────────────────────────── */
char*     get_saved_data_from_flash(const char* namespace, const char* key);
esp_err_t saved_data_in_flash(const char* namespace, const char* key,
                               const char* value);

/* ── GPS coordinate persistence ──────────────────────────────────────────── */
esp_err_t load_saved_gps_from_nvs(void);

/* ── uint32 helpers (pulse counters, etc.) ───────────────────────────────── */
esp_err_t nvs_save_u32(const char *ns, const char *key, uint32_t value);
esp_err_t nvs_load_u32(const char *ns, const char *key, uint32_t *out);

/* ── NVS initialisation ──────────────────────────────────────────────────── */
esp_err_t init_nvs(void);

#endif /* NVS_MANAGER_H */