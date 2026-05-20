/*
 * Copyright (c) 2024 <[SFM Technologies]>
 *
 * Storage backend: SPIFFS (internal flash, "storage" partition, ~1.4 MB)
 *
 * SPIFFS notes
 * ─────────────
 *  • No sub-directory support — all files live in MOUNT_POINT root.
 *  • CONFIG_SPIFFS_OBJ_NAME_LEN must be >= 48 (set to 64 in sdkconfig).
 *  • ~1.4 MB available for failed MQTT payloads.
 */

#include "LOG_Save_SD_Manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG     = "SD_MANAGER";
static bool        s_mounted = false;

/* Global flag: true when at least one failed payload is stored on SPIFFS */
volatile bool g_sd_has_pending_payloads = false;

/* ── Timestamp helper ────────────────────────────────────────────────────── */

static void get_timestamp_string(char *buf, size_t buf_size, bool for_filename)
{
    if (!buf || buf_size == 0) return;

    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info)) {
        strftime(buf, buf_size,
                 for_filename ? "%Y%m%d_%H%M%S" : "%Y-%m-%d %H:%M:%S",
                 &tm_info);
    } else {
        uint64_t up = esp_timer_get_time() / 1000000ULL;
        snprintf(buf, buf_size,
                 for_filename ? "uptime_%llu" : "Uptime: %llu s", up);
    }
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

esp_err_t sd_card_init(void)
{
    if (s_mounted) {
        ESP_LOGW(TAG, "SPIFFS already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting SPIFFS (partition: \"%s\", base: \"%s\")",
             SPIFFS_PARTITION_LABEL, MOUNT_POINT);

    esp_vfs_spiffs_conf_t conf = {
        .base_path              = MOUNT_POINT,
        .partition_label        = SPIFFS_PARTITION_LABEL,
        .max_files              = SD_MAX_FILES,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Print partition info */
    size_t total = 0, used = 0;
    if (esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS ready — total: %u B  used: %u B  free: %u B",
                 (unsigned)total, (unsigned)used, (unsigned)(total - used));
    }

    s_mounted = true;
    g_sd_has_pending_payloads = sd_has_pending_payloads();
    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_mounted) {
        ESP_LOGW(TAG, "SPIFFS not mounted");
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_spiffs_unregister(SPIFFS_PARTITION_LABEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS unmount failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }
    s_mounted = false;
    return ret;
}

bool sd_card_is_mounted(void)
{
    return s_mounted;
}

/* ── File operations ─────────────────────────────────────────────────────── */

esp_err_t sd_write_json(const char *file_path, const char *json_data)
{
    if (!file_path || !json_data) return ESP_ERR_INVALID_ARG;
    if (!s_mounted) {
        ESP_LOGE(TAG, "sd_write_json: SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    FILE *f = fopen(file_path, "a");
    if (!f) {
        f = fopen(file_path, "w");
        if (!f) {
            ESP_LOGE(TAG, "sd_write_json: cannot open %s", file_path);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "sd_write_json: created %s", file_path);
    }

    int n = fprintf(f, "%s\n", json_data);
    fclose(f);

    if (n < 0) {
        ESP_LOGE(TAG, "sd_write_json: write failed");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "sd_write_json: wrote %d bytes to %s", n, file_path);
    return ESP_OK;
}

esp_err_t sd_delete_file(const char *file_path)
{
    if (!file_path) return ESP_ERR_INVALID_ARG;
    if (!s_mounted) {
        ESP_LOGE(TAG, "sd_delete_file: SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    if (unlink(file_path) == 0) {
        ESP_LOGI(TAG, "Deleted: %s", file_path);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Delete failed: %s (errno %d)", file_path, errno);
    return ESP_FAIL;
}

esp_err_t sd_save_failed_payload(const char *payload, int error_code)
{
    if (!payload) return ESP_ERR_INVALID_ARG;

    /* Skip if clock is not yet synchronised */
    if (time(NULL) < 1577836800) {
        ESP_LOGW(TAG, "Time not synchronised — skipping SPIFFS save");
        return ESP_ERR_INVALID_STATE;
    }

    /* Mount lazily if needed */
    if (!s_mounted) {
        esp_err_t ret = sd_card_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "sd_save_failed_payload: SPIFFS unavailable: %s",
                     esp_err_to_name(ret));
            return ret;
        }
    }

    /* Append one reading (one JSON line) to the shared readings file */
    FILE *f = fopen(READINGS_FILE, "a");
    if (!f) {
        f = fopen(READINGS_FILE, "w");   /* create if it doesn't exist yet */
        if (!f) {
            ESP_LOGE(TAG, "sd_save_failed_payload: cannot open %s", READINGS_FILE);
            return ESP_FAIL;
        }
    }

    int written = fprintf(f, "%s\n", payload);
    fclose(f);

    if (written <= 0) {
        ESP_LOGE(TAG, "sd_save_failed_payload: write error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Reading appended to %s (%d bytes)", READINGS_FILE, written);
    g_sd_has_pending_payloads = true;

    /* ── Debug dump: print the full file content after every save ────────────
     * Only compiled/shown when log level is DEBUG (idf.py menuconfig →
     * Component config → Log output → Default log verbosity → Debug,
     * or call esp_log_level_set("SD_MANAGER", ESP_LOG_DEBUG) at runtime).  */
#if CONFIG_LOG_DEFAULT_LEVEL >= 4   /* 4 = DEBUG */
    {
        FILE *dbg = fopen(READINGS_FILE, "r");
        if (dbg) {
            ESP_LOGD(TAG, "┌──────────────── %s contents ────────────────┐", READINGS_FILE);
            char *dbg_line = malloc(2048);
            if (dbg_line) {
                int line_no = 1;
                while (fgets(dbg_line, 2048, dbg)) {
                    /* strip trailing newline for clean output */
                    size_t ll = strlen(dbg_line);
                    while (ll > 0 && (dbg_line[ll-1] == '\n' || dbg_line[ll-1] == '\r'))
                        dbg_line[--ll] = '\0';
                    ESP_LOGD(TAG, "│ [%02d] %s", line_no++, dbg_line);
                }
                free(dbg_line);
            }
            fclose(dbg);

            struct stat st;
            size_t total_sz = (stat(READINGS_FILE, &st) == 0) ? (size_t)st.st_size : 0;
            ESP_LOGD(TAG, "└────────────────── total: %u bytes ─────────────────┘",
                     (unsigned)total_sz);
        }
    }
#endif  /* CONFIG_LOG_DEFAULT_LEVEL >= 4 */

    return ESP_OK;
}

esp_err_t sd_create_directory(const char *path)
{
    /* SPIFFS has no sub-directory support — silently succeed */
    (void)path;
    return ESP_OK;
}

esp_err_t sd_list_files(const char *path, char ***file_list, int *file_count)
{
    if (!path || !file_list || !file_count) return ESP_ERR_INVALID_ARG;
    if (!s_mounted) {
        ESP_LOGE(TAG, "sd_list_files: SPIFFS not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "sd_list_files: cannot open %s", path);
        return ESP_FAIL;
    }

    /* Count regular files */
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) count++;
    }

    char **list = malloc(count * sizeof(char *));
    if (!list) { closedir(dir); return ESP_ERR_NO_MEM; }

    rewinddir(dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < count) {
        if (entry->d_type == DT_REG) {
            list[idx] = strdup(entry->d_name);
            if (!list[idx]) {
                for (int i = 0; i < idx; i++) free(list[i]);
                free(list);
                closedir(dir);
                return ESP_ERR_NO_MEM;
            }
            idx++;
        }
    }
    closedir(dir);

    *file_list  = list;
    *file_count = count;
    return ESP_OK;
}

void sd_free_file_list(char **file_list, int file_count)
{
    if (!file_list) return;
    for (int i = 0; i < file_count; i++) free(file_list[i]);
    free(file_list);
}

size_t sd_get_file_size(const char *file_path)
{
    if (!file_path || !s_mounted) return 0;
    struct stat st;
    return (stat(file_path, &st) == 0) ? (size_t)st.st_size : 0;
}

bool sd_file_exists(const char *file_path)
{
    if (!file_path || !s_mounted) return false;
    struct stat st;
    return stat(file_path, &st) == 0;
}

/* ── Pending-payload retry ───────────────────────────────────────────────── */

#define SPIFFS_RETRY_MAX_FILES  32
#define SPIFFS_RETRY_NAME_LEN   64

bool sd_has_pending_payloads(void)
{
    if (!s_mounted) return g_sd_has_pending_payloads;

    /* Single-file model: pending data exists when readings.json is non-empty */
    struct stat st;
    bool has = (stat(READINGS_FILE, &st) == 0 && st.st_size > 0);
    g_sd_has_pending_payloads = has;
    return has;
}

esp_err_t sd_retry_pending_payloads(sd_retry_publish_cb_t publish_cb)
{
    if (!publish_cb) return ESP_ERR_INVALID_ARG;

    if (!s_mounted) {
        esp_err_t ret = sd_card_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "sd_retry: SPIFFS unavailable: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    FILE *f = fopen(READINGS_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG, "sd_retry: no readings file — nothing to retry");
        g_sd_has_pending_payloads = false;
        return ESP_OK;
    }

    /* Read line by line; each line is one complete JSON payload */
    char *line = malloc(8192);
    if (!line) { fclose(f); return ESP_ERR_NO_MEM; }

    int sent = 0;
    while (fgets(line, 8192, f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (len == 0) continue;

        ESP_LOGI(TAG, "sd_retry: replaying reading %d", sent + 1);
        publish_cb(line);
        sent++;
    }
    free(line);
    fclose(f);

    /* Delete the file after replaying all readings */
    sd_delete_file(READINGS_FILE);
    g_sd_has_pending_payloads = false;

    ESP_LOGI(TAG, "sd_retry: done — %d reading(s) replayed, file deleted", sent);
    return ESP_OK;
}
