/*
 * Copyright (c) 2024 <[SFM Technologies]>
 *
 * Storage backend: SPIFFS (internal flash, "storage" partition, ~1.4 MB)
 */

#ifndef LOG_SAVE_SD_MANAGER_H
#define LOG_SAVE_SD_MANAGER_H

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Filesystem config ───────────────────────────────────────────────────── */

/** VFS mount point (same as before so all callers stay unchanged). */
#define MOUNT_POINT             "/spiffs"

/** Partition label — must match the "Name" column in partitions.csv. */
#define SPIFFS_PARTITION_LABEL  "storage"

/** Maximum simultaneously open files. */
#define SD_MAX_FILES            5

/**
 * Single file that stores all offline readings (one JSON object per line).
 * Replaces the per-reading failed_mqtt_*.json approach.
 */
#define READINGS_FILE           MOUNT_POINT "/readings.json"

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/** Mount SPIFFS.  Safe to call multiple times (idempotent). */
esp_err_t sd_card_init(void);

/** Unmount SPIFFS. */
esp_err_t sd_card_deinit(void);

/** Returns true when SPIFFS is mounted and ready. */
bool sd_card_is_mounted(void);

/* ── File operations ─────────────────────────────────────────────────────── */

/** Append a JSON string (one line) to file_path. */
esp_err_t sd_write_json(const char *file_path, const char *json_data);

/** Delete a file. */
esp_err_t sd_delete_file(const char *file_path);

/**
 * @brief Persist a failed MQTT payload to SPIFFS so it can be retried later.
 *
 * Skipped silently when the system clock is not yet synchronised (time < 2020).
 */
esp_err_t sd_save_failed_payload(const char *payload, int error_code);

/**
 * @brief No-op — SPIFFS does not support sub-directories.
 *        Returns ESP_OK so existing callers compile without change.
 */
esp_err_t sd_create_directory(const char *path);

/** List regular files in path.  Caller must free with sd_free_file_list(). */
esp_err_t sd_list_files(const char *path, char ***file_list, int *file_count);

/** Free a file list allocated by sd_list_files(). */
void sd_free_file_list(char **file_list, int file_count);

/** Return the size (bytes) of a file, or 0 on error / not found. */
size_t sd_get_file_size(const char *file_path);

/** Return true if the file exists. */
bool sd_file_exists(const char *file_path);

/* ── Pending-payload retry API ───────────────────────────────────────────── */

/**
 * Global flag — set whenever a payload is saved because MQTT was unavailable.
 * Cleared automatically by sd_retry_pending_payloads().
 */
extern volatile bool g_sd_has_pending_payloads;

/**
 * Callback type for sd_retry_pending_payloads().
 * Receives the raw payload string; the caller rebuilds the MQTT topic.
 */
typedef void (*sd_retry_publish_cb_t)(const char *payload);

/** Returns true when at least one failed_mqtt_* file exists on SPIFFS. */
bool sd_has_pending_payloads(void);

/**
 * @brief Iterate every failed_mqtt_* file, pass each payload to publish_cb,
 *        then delete the file.
 *
 * @param publish_cb  Must not be NULL.
 */
esp_err_t sd_retry_pending_payloads(sd_retry_publish_cb_t publish_cb);

#ifdef __cplusplus
}
#endif

#endif /* LOG_SAVE_SD_MANAGER_H */
