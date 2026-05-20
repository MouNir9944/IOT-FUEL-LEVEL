// modem_mqtt_handler.c
#include "modem_event_handler.h"
#include "modem_mqtt_handler.h"
#include "SIM7600.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include "cJSON.h"
#include "Sensor_Fuel_Manager.h"
#include "SIM7600_MQTT.h"
#include "LOG_Save_SD_Manager.h"
#include "SIM7600_HTTP.h"
#include "device_config.h"

/* ── Offline-sync HTTP endpoint ─────────────────────────────────────────────
 * When internet recovers the device POSTs all SD-buffered readings in one
 * batch to this endpoint instead of replaying them one-by-one over MQTT.   */
#define SYNC_SERVER   "smart-gridix-backend.onrender.com"
#define SYNC_PATH     "/api/devices/sync"
#define SYNC_BATCH_SIZE  8   /* readings per HTTP POST — keeps body under ~3 KB   */
static const char *TAG_MODEM_MQTT_HANDLER = "MODEM MQTT HANDLER";
extern void mqtt_main_task(void *pvParameters);
TaskHandle_t mqtt_main_task_handler = NULL;

/**
 * @brief Callback passed to sd_retry_pending_payloads().
 *        Called for each failed payload recovered from the SD card.
 *        Runs inside mqtt_command_task so it is safe to call
 *        SIM7600_mqtt_publish() directly.
 */
static void retry_publish_cb(const char *payload) {
    if (!payload) return;
    const char *mac = get_device_mac_str();
    char topic[128];
    snprintf(topic, sizeof(topic),
             "device/%s/telemetry", mac);
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "SD retry → %s", topic);
    SIM7600_mqtt_publish(topic, payload);
}

/**
 * @brief Batch all SD-buffered readings and POST them to the offline-sync
 *        HTTP endpoint in one request.  Files are deleted only on success.
 *
 * Replaces sd_retry_pending_payloads() for the HTTP recovery path.
 *
 * @param mac  Device MAC address string (e.g. "1C:69:20:35:13:90")
 */
static void http_sync_pending_payloads(const char *mac) {
    if (!mac) return;

    /* ── Load all pending readings from SPIFFS into memory ─────────────────────
     * We read every line first, then send in batches of SYNC_BATCH_SIZE.
     * Batching keeps each HTTP POST body under ~3 KB (proven reliable).
     * The server accepts duplicates gracefully, so retries are safe.        */
    FILE *f = fopen(READINGS_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "[HTTP-SYNC] No readings file — nothing to sync");
        g_sd_has_pending_payloads = false;
        return;
    }

    /* Collect all valid JSON lines into a dynamic array */
    #define HTTP_SYNC_MAX_LINES 256
    char **lines     = calloc(HTTP_SYNC_MAX_LINES, sizeof(char *));
    int    total     = 0;
    char  *linebuf   = malloc(8192);

    if (!lines || !linebuf) {
        fclose(f);
        free(lines);
        free(linebuf);
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "[HTTP-SYNC] OOM loading readings");
        return;
    }

    while (fgets(linebuf, 8192, f) && total < HTTP_SYNC_MAX_LINES) {
        size_t len = strlen(linebuf);
        while (len > 0 && (linebuf[len-1] == '\n' || linebuf[len-1] == '\r'))
            linebuf[--len] = '\0';
        if (len == 0) continue;

        /* Validate it is parseable JSON before queuing */
        cJSON *test = cJSON_Parse(linebuf);
        if (test) {
            cJSON_Delete(test);
            lines[total] = strdup(linebuf);
            if (lines[total]) total++;
        } else {
            ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "[HTTP-SYNC] Skipping corrupt line");
        }
    }
    free(linebuf);
    fclose(f);

    if (total == 0) {
        for (int i = 0; i < total; i++) free(lines[i]);
        free(lines);
        sd_delete_file(READINGS_FILE);
        g_sd_has_pending_payloads = false;
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "[HTTP-SYNC] No valid readings to send");
        return;
    }

    ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
             "[HTTP-SYNC] %d reading(s) queued — posting in batch(es) of %d",
             total, SYNC_BATCH_SIZE);

    /* ── Send in batches ────────────────────────────────────────────────────────
     * Stops on first failed batch; remaining readings stay in the file and
     * will be retried on next reconnect.                                     */
    int sent_total = 0;
    bool all_ok    = true;

    for (int batch_start = 0; batch_start < total && all_ok; batch_start += SYNC_BATCH_SIZE) {
        int batch_end = batch_start + SYNC_BATCH_SIZE;
        if (batch_end > total) batch_end = total;
        int batch_count = batch_end - batch_start;

        /* Build  {"mac":"...","readings":[...]}  for this batch */
        cJSON *root     = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "mac", mac);
        cJSON *readings = cJSON_AddArrayToObject(root, "readings");

        for (int i = batch_start; i < batch_end; i++) {
            cJSON *item = cJSON_Parse(lines[i]);
            if (item) cJSON_AddItemToArray(readings, item);
        }

        char *json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!json_str) {
            ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "[HTTP-SYNC] JSON serialization OOM (batch %d)", batch_start);
            all_ok = false;
            break;
        }

        ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                 "[HTTP-SYNC] Posting readings %d–%d of %d to https://%s%s",
                 batch_start + 1, batch_end, total, SYNC_SERVER, SYNC_PATH);

        esp_err_t ret = SIMcom_https_post_multipart_SIM7600(
            (char *)SYNC_SERVER, 0, (char *)SYNC_PATH, json_str, false);
        free(json_str);

        if (ret == ESP_OK) {
            sent_total += batch_count;
            ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                     "[HTTP-SYNC] ✓ Batch delivered (%d/%d total sent)",
                     sent_total, total);
        } else {
            ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                     "[HTTP-SYNC] ✗ Batch failed (%s) — stopping, %d/%d sent",
                     esp_err_to_name(ret), sent_total, total);
            all_ok = false;
        }
    }

    /* Free the in-memory line array */
    for (int i = 0; i < total; i++) free(lines[i]);
    free(lines);

    /* ── Delete file only when every reading was delivered ──────────────────── */
    if (sent_total == total) {
        sd_delete_file(READINGS_FILE);
        g_sd_has_pending_payloads = false;
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                 "[HTTP-SYNC] ✓ All %d reading(s) delivered — %s deleted",
                 total, READINGS_FILE);
    } else {
        /* Partial success — keep file (server deduplicates on next retry) */
        ESP_LOGW(TAG_MODEM_MQTT_HANDLER,
                 "[HTTP-SYNC] Partial sync: %d/%d sent — file kept for retry",
                 sent_total, total);
    }
}
extern TaskHandle_t status_monitor_task_handle;
extern bool connected;
extern bool subscribe_status;
// Event base for MQTT events
ESP_EVENT_DEFINE_BASE(MQTT_EVENTS);
float last_fuel_level =0.0f;
float last_temperature = 0.0f;
int   last_rssi = -100;
// MQTT context structure
typedef struct {
    char broker_url[128];
    char username[64];
    char password[64];
    char client_id[64];
    char will_topic[64];
    char will_message[64];
    uint16_t keep_alive;
    uint8_t qos;
    bool clean_session;
    bool is_connected;
    TaskHandle_t mqtt_task;
    QueueHandle_t command_queue;
    EventGroupHandle_t event_group;
    TimerHandle_t reconnect_timer;
    uint8_t reconnect_attempts;
    uint8_t no_signal_fail_count;   /* consecutive publish failures with signal == -1 */
} mqtt_context_t;

static mqtt_context_t mqtt_ctx = {
    .broker_url = "broker.hivemq.com:1883",
    .username = "",
    .password = "",
    .client_id = "ESP32_SIM7000",
    .will_topic = "esp32/sim7000/status",
    .will_message = "offline",
    .keep_alive = 60,
    .qos = 1,
    .clean_session = true,
    .is_connected = false,
    .reconnect_attempts = 0
};

// Command definitions for MQTT operations
typedef enum {
    MQTT_CMD_CONNECT,
    MQTT_CMD_DISCONNECT,
    MQTT_CMD_PUBLISH,
    MQTT_CMD_PUBLISH_LOGS,
    MQTT_CMD_SUBSCRIBE,
    MQTT_CMD_UNSUBSCRIBE,
    MQTT_CMD_RECONNECT,
    MQTT_CMD_READY,
    MQTT_CMD_MESSAGE_RECEIVED,
    CMD_OTA,
    UPDATE_CONFIG,
} mqtt_cmd_t;

typedef struct {
    mqtt_cmd_t cmd;
    union {
        struct {
            char topic[128];
            char payload[512];
            uint8_t qos;
        } publish;
        struct {
            char topic[128];
            uint8_t qos;
        } subscribe;
        struct {
            char topic[128];
        } unsubscribe;
        struct {
            char *topic;    // Changed to pointer for dynamic allocation
            char *payload;  // Changed to pointer for dynamic allocation
        } receive_msg;
        struct {
            char config_json[512];
        } update_config;
        struct {
            char ota_url[256];
        } ota;
    } data;
} mqtt_command_t;

// Event group bits
#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_RECONNECT_BIT      BIT1
#define MQTT_STOP_BIT           BIT2

// Forward declarations
static void mqtt_command_task(void *pvParameters);
static void mqtt_reconnect_timer_callback(TimerHandle_t xTimer);
static void mqtt_message_callback(const char *topic, const char *payload);

/**
 * @brief Initialize MQTT handler
 */
esp_err_t mqtt_handler_init(void) {
    /* Re-init guard: called on every reconnect via ESP_ERROR_CHECK in the
     * event handler.  Without this guard every reconnect leaks an event group,
     * a queue, a timer, and a 12 KB task stack.  After enough reconnects the
     * heap exhausts → xQueueCreate returns NULL → ESP_ERROR_CHECK aborts. */
    if (mqtt_ctx.command_queue != NULL) {
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "MQTT handler already initialized — skipping re-init");
        return ESP_OK;
    }

    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Initializing MQTT handler...");

    // Create event group
    mqtt_ctx.event_group = xEventGroupCreate();
    if (!mqtt_ctx.event_group) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Create command queue
    mqtt_ctx.command_queue = xQueueCreate(10, sizeof(mqtt_command_t));
    if (!mqtt_ctx.command_queue) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to create command queue");
        vEventGroupDelete(mqtt_ctx.event_group);
        return ESP_ERR_NO_MEM;
    }
    
    // Create reconnect timer
    mqtt_ctx.reconnect_timer = xTimerCreate(
        "mqtt_reconnect",
        pdMS_TO_TICKS(30000),  // 30 seconds initial reconnect delay
        pdFALSE,  // One-shot timer
        NULL,
        mqtt_reconnect_timer_callback
    );
    
    if (!mqtt_ctx.reconnect_timer) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to create reconnect timer");
        vQueueDelete(mqtt_ctx.command_queue);
        vEventGroupDelete(mqtt_ctx.event_group);
        return ESP_ERR_NO_MEM;
    }
    
    // Create MQTT command task (will be suspended until needed)
    xTaskCreate(mqtt_command_task, "mqtt_cmd", 12288, NULL, 5, &mqtt_ctx.mqtt_task);
    vTaskSuspend(mqtt_ctx.mqtt_task);  // Start suspended
    
    // Register MQTT message callback
    SIM7600_mqtt_set_message_callback(mqtt_message_callback);
    
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "MQTT handler initialized (suspended)");
    return ESP_OK;
}

/**
 * @brief Start MQTT handler (call this when data is connected)
 */
esp_err_t mqtt_handler_start(void) {
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Starting MQTT handler...");
    
    if (!mqtt_ctx.mqtt_task) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "MQTT handler not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Reset reconnect attempts
    mqtt_ctx.reconnect_attempts = 0;
    
    // Resume MQTT task
    vTaskResume(mqtt_ctx.mqtt_task);
    
    // Queue initial connection
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_CONNECT
    };
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue connect command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Stop MQTT handler
 */
esp_err_t mqtt_handler_stop(void) {
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Stopping MQTT handler...");
    
    // Stop reconnect timer
    xTimerStop(mqtt_ctx.reconnect_timer, 0);
    
    // Disconnect if connected
    if (mqtt_ctx.is_connected) {
        SIM7600_mqtt_disconnect();
        mqtt_ctx.is_connected = false;
        
        // Post disconnect event
        esp_event_post(MQTT_EVENTS, MQTT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
    }
    
    // Clear event group
    xEventGroupClearBits(mqtt_ctx.event_group, MQTT_CONNECTED_BIT | MQTT_RECONNECT_BIT);
    
    // Suspend task
    vTaskSuspend(mqtt_ctx.mqtt_task);
    
    return ESP_OK;
}

/**
 * @brief Set MQTT broker configuration
 */
esp_err_t mqtt_set_broker(const char *broker, const char *username, const char *password) {
    if (!broker) return ESP_ERR_INVALID_ARG;
    
    strncpy(mqtt_ctx.broker_url, broker, sizeof(mqtt_ctx.broker_url) - 1);
    
    if (username) {
        strncpy(mqtt_ctx.username, username, sizeof(mqtt_ctx.username) - 1);
    }
    
    if (password) {
        strncpy(mqtt_ctx.password, password, sizeof(mqtt_ctx.password) - 1);
    }
    
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Broker set to: %s", broker);
    return ESP_OK;
}

/**
 * @brief Set MQTT client ID
 */
esp_err_t mqtt_set_client_id(const char *client_id) {
    if (!client_id) return ESP_ERR_INVALID_ARG;
    
    strncpy(mqtt_ctx.client_id, client_id, sizeof(mqtt_ctx.client_id) - 1);
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Client ID set to: %s", client_id);
    return ESP_OK;
}

/**
 * @brief Set MQTT will message
 */
esp_err_t mqtt_set_will(const char *topic, const char *message) {
    if (!topic || !message) return ESP_ERR_INVALID_ARG;
    
    strncpy(mqtt_ctx.will_topic, topic, sizeof(mqtt_ctx.will_topic) - 1);
    strncpy(mqtt_ctx.will_message, message, sizeof(mqtt_ctx.will_message) - 1);
    
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Will set: %s -> %s", topic, message);
    return ESP_OK;
}

/**
 * @brief Publish MQTT message (non-blocking)
 */
esp_err_t mqtt_publish(const char *topic, const char *payload, uint8_t qos) {
    //if (!topic || !payload) return ESP_ERR_INVALID_ARG;
    
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_PUBLISH,
        .data.publish.qos = qos
    };
    
    strncpy(cmd.data.publish.topic, topic, sizeof(cmd.data.publish.topic) - 1);
    strncpy(cmd.data.publish.payload, payload, sizeof(cmd.data.publish.payload) - 1);
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue publish command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
/**
 * @brief Publish MQTT message (non-blocking)
 */
esp_err_t mqtt_publish_logs(const char *topic, const char *err,const char *code,const char *ermessage, uint8_t qos) {
    /* Guard: queue may be NULL when modem is restarting / not yet initialized.
     * xQueueSend() asserts (panics) on a NULL handle, so we must return early. */
    if (!mqtt_ctx.command_queue || !mqtt_ctx.is_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_PUBLISH_LOGS,
        .data.publish.qos = qos
    };
        /* ── Timestamp ─────────────────────────────────────────────────────────── */
    time_t now = 0;
    time(&now);
    
    strncpy(cmd.data.publish.topic, topic, sizeof(cmd.data.publish.topic) - 1);
   /* ── Build telemetry JSON ──────────────────────────────────────────────── */
    cJSON *logs_msg = cJSON_CreateObject();

    cJSON_AddStringToObject(logs_msg, "level",   err);
    cJSON_AddStringToObject(logs_msg, "code",   code);
    cJSON_AddStringToObject(logs_msg, "message",   ermessage);
    cJSON_AddNumberToObject(logs_msg, "ts",   (double)(long)now);
    char* payload=NULL;
    payload = cJSON_PrintUnformatted(logs_msg);
    strncpy(cmd.data.publish.payload, payload, sizeof(cmd.data.publish.payload) - 1);
    free(payload);
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue publish command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Subscribe to MQTT topic (non-blocking)
 */
esp_err_t mqtt_ready(const char *topic, uint8_t qos) {
    if (!topic) return ESP_ERR_INVALID_ARG;
    
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_READY,
        .data.subscribe.qos = qos
    };
    
    strncpy(cmd.data.subscribe.topic, topic, sizeof(cmd.data.subscribe.topic) - 1);
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue ready command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Subscribe to MQTT topic (non-blocking)
 */
esp_err_t start_ota_update(const char *url) {

    mqtt_command_t cmd = {
        .cmd = CMD_OTA,
    };
    if (url) {
        strncpy(cmd.data.ota.ota_url, url, sizeof(cmd.data.ota.ota_url) - 1);
        cmd.data.ota.ota_url[sizeof(cmd.data.ota.ota_url) - 1] = '\0';
    } else {
        cmd.data.ota.ota_url[0] = '\0';
    }

    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue OTA command");
        return ESP_FAIL;
    }

    return ESP_OK;
}
/**
 * @brief Subscribe to MQTT topic (non-blocking)
 */
esp_err_t start_update_config(const char *config_json) {

    mqtt_command_t cmd = {
        .cmd = UPDATE_CONFIG,
    };
    if (config_json) {
        strncpy(cmd.data.update_config.config_json, config_json,
                sizeof(cmd.data.update_config.config_json) - 1);
        cmd.data.update_config.config_json[sizeof(cmd.data.update_config.config_json) - 1] = '\0';
    } else {
        cmd.data.update_config.config_json[0] = '\0';
    }

    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue ready command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
/**
 * @brief Subscribe to MQTT topic (non-blocking)
 */
esp_err_t mqtt_subscribe(const char *topic, uint8_t qos) {
    if (!topic) return ESP_ERR_INVALID_ARG;
    
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_SUBSCRIBE,
        .data.subscribe.qos = qos
    };
    
    strncpy(cmd.data.subscribe.topic, topic, sizeof(cmd.data.subscribe.topic) - 1);
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue subscribe command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
/**
 * @brief Unsubscribe from MQTT topic
 */
esp_err_t mqtt_unsubscribe(const char *topic) {
    if (!topic) return ESP_ERR_INVALID_ARG;
    
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_UNSUBSCRIBE
    };
    
    strncpy(cmd.data.unsubscribe.topic, topic, sizeof(cmd.data.unsubscribe.topic) - 1);
    
    if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue unsubscribe command");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Check if MQTT is connected
 */
bool mqtt_is_connected(void) {
    return mqtt_ctx.is_connected;
}

/**
 * @brief Wait for MQTT connection
 */
esp_err_t mqtt_wait_for_connection(TickType_t timeout) {
    EventBits_t bits = xEventGroupWaitBits(
        mqtt_ctx.event_group,
        MQTT_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        timeout
    );
    
    return (bits & MQTT_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}
void mqtt_main_task(void *pvParameters) {

    /* Build correct topics from real device MAC */
    const char *mac = get_device_mac_str();
    char telemetry_topic[128];
    char config_topic[128];
    char ota_topic[128];
    char logs_topic[128];
    char cmd_topic[128];

    snprintf(telemetry_topic, sizeof(telemetry_topic),
             "device/%s/telemetry", mac);
    snprintf(logs_topic,      sizeof(logs_topic),
             "device/%s/logs", mac);
    snprintf(config_topic,    sizeof(config_topic),
             "device/%s/config", mac);
    snprintf(ota_topic,       sizeof(ota_topic),
             "device/%s/ota", mac);
    snprintf(cmd_topic,       sizeof(cmd_topic),
             "device/%s/cmd", mac);
    /* Publish loop */
    while (1) {
        char *payload = NULL; 
        cJSON *payload_str = NULL;
        
        create_and_send_json_request_soji_sensor_fuel_level(&payload_str);
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        if (payload_str != NULL) {
            cJSON *level_item = cJSON_GetObjectItem(payload_str, "level_cm");
            cJSON *temp_item  = cJSON_GetObjectItem(payload_str, "temp_c");
            cJSON *rssi_item  = cJSON_GetObjectItem(payload_str, "rssi");

            if (level_item && temp_item && rssi_item) {
                if (last_fuel_level != (float)level_item->valuedouble || 
                    last_temperature != (float)temp_item->valuedouble || 
                    last_rssi != rssi_item->valueint) {
                    
                    last_fuel_level = (float)level_item->valuedouble;
                    last_temperature = (float)temp_item->valuedouble;
                    last_rssi = rssi_item->valueint;
                    
                    payload = cJSON_PrintUnformatted(payload_str);
                    
                    if (mqtt_ctx.is_connected) {
                        /* Subscribe to backend → device config topic */
                        if (subscribe_status == false) {
                            mqtt_subscribe(config_topic, 0);
                            mqtt_subscribe(ota_topic, 0);
                            mqtt_subscribe(cmd_topic, 1);
                            subscribe_status = true;
                        }
                        mqtt_publish(telemetry_topic, payload, 1);
                    } else {
                        ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "MQTT not connected — saving to SD");
                        if (payload) {
                            esp_err_t save_ret = sd_save_failed_payload(
                                    payload, ESP_FAIL);
                            if (save_ret == ESP_OK) {
                                ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                                         "Payload stored on SD (pending retry)");
                            } else {
                                ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                         "Failed to store payload on SD: %s",
                                         esp_err_to_name(save_ret));
                            }
                        }
                    }
                    
                    if (payload) {
                        free(payload);
                        payload = NULL;
                    }
                } else {
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "No change in sensor data — skipping publish");
                    if (mqtt_ctx.is_connected) {
                        mqtt_publish_logs(logs_topic, "Failed", "NO_CHANGE",
                                          "No change in sensor data — skipping publish", 1);
                    }
                }
            } else {
                ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Invalid JSON structure");
                if (mqtt_ctx.is_connected) {
                    mqtt_publish_logs(logs_topic, "Failed", "INVALID_JSON",
                                      "Invalid JSON structure", 1);
                }
            }
            
            /* Delete cJSON object only once */
            cJSON_Delete(payload_str);
            payload_str = NULL;
            
        } else {
            ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Sensor read failed — telemetry skipped");
            if (mqtt_ctx.is_connected) {
            mqtt_publish_logs(logs_topic,"Failed","INVALID_JSON","Sensor read failed — telemetry skipped", 1);
            }
            free(payload);
        }
        vTaskDelay(pdMS_TO_TICKS(g_device_config.reporting_interval_s * 1000));
    }
}
/**
 * @brief Start the status monitor (called after MQTT connected)
 */
esp_err_t mqtt_main_task_start(void) {
    if (mqtt_main_task_handler) {
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Starting modem status monitor");
        vTaskResume(mqtt_main_task_handler);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Stop the status monitor
 */
esp_err_t mqtt_main_task_stop(void) {
    if (mqtt_main_task_handler) {
        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Stopping modem status monitor");
        vTaskSuspend(mqtt_main_task_handler);
        return ESP_OK;
    }
    return ESP_FAIL;
}
/**
 * @brief MQTT command task
 */
static void mqtt_command_task(void *pvParameters) {
    mqtt_command_t cmd;
    
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "MQTT command task started");
    
    while (1) {
        if (xQueueReceive(mqtt_ctx.command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.cmd) {
                case MQTT_CMD_CONNECT: {
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Deinit to MQTT broker");
                   //SIM7600_mqtt_disconnect();
                   // sim7600_mqtt_stop();
                    if(sim7600_mqtt_deinit()!=ESP_OK){
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Disconnect to MQTT broker");
                       // SIM7600_mqtt_disconnect();
                    }
                     vTaskDelay(pdMS_TO_TICKS(3000)); // Publish every 20 seconds
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Connecting to MQTT broker: %s", mqtt_ctx.broker_url);


                    /* Set up LWT and acquire client slot */
                    SIM7600_mqtt_connect("smartsensor");

                    /* Register LWT AFTER AT+CMQTTACCQ, BEFORE AT+CMQTTCONNECT.
                     * The broker will publish this automatically when the device
                     * loses its TCP connection (power loss, keepalive timeout, etc.) */
                    {
                        const char *mac = get_device_mac_str();
                        char will_topic[128];
                        snprintf(will_topic, sizeof(will_topic),
                                 "device/%s/status", mac);
                        SIM7600_mqtt_set_will(will_topic, "offline", 1);
                    }

                    /* Retry MQTT CONNECT up to 5 times before giving up */
                    bool conn_ok = false;
                    while (SIM7600_mqtt_set_broker("tcp://broker.hivemq.com:1883") != ESP_OK) {
                        mqtt_ctx.is_connected = false;
                        mqtt_ctx.reconnect_attempts++;
                        ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                 "MQTT connection failed (attempt %d/5)",
                                 mqtt_ctx.reconnect_attempts);
                        if (mqtt_ctx.reconnect_attempts >= 5) {
                            ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                     "MQTT reconnect attempts exceeded, resetting data link");
                            modem_connect_data();
                            connected = false;
                            break;          /* conn_ok stays false */
                        }
                        vTaskDelay(pdMS_TO_TICKS(3000)); /* wait before retry */
                    }

                    /* while exits normally only when broker returns ESP_OK */
                    if (mqtt_ctx.reconnect_attempts < 5) {
                        conn_ok = true;
                    }

                    if (conn_ok) {
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "MQTT connected successfully!");
                        xEventGroupSetBits(mqtt_ctx.event_group, MQTT_CONNECTED_BIT);
                        mqtt_ctx.is_connected = true;
                        mqtt_ctx.reconnect_attempts = 0;

                        /* ── Send any payloads buffered on SD via HTTP sync ── */
                        if (g_sd_has_pending_payloads) {
                            ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                                     "Pending SD payloads — syncing via HTTP...");
                            http_sync_pending_payloads(get_device_mac_str());
                        }

                        mqtt_command_t ready_cmd = { .cmd = MQTT_CMD_READY };
                        if (xQueueSend(mqtt_ctx.command_queue, &ready_cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
                            ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue READY command");
                        }
                    }
                    break;
                }                   
                case MQTT_CMD_DISCONNECT:
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Disconnecting from MQTT broker");
                    // "offline" is delivered automatically by the broker via LWT —
                    // do NOT publish it manually here.
                    
                    if (mqtt_ctx.is_connected) {
                        SIM7600_mqtt_disconnect();
                    }
                    mqtt_ctx.is_connected = false;
                    xEventGroupClearBits(mqtt_ctx.event_group, MQTT_CONNECTED_BIT);
                    break;
                    
                case MQTT_CMD_PUBLISH:
                    if (mqtt_ctx.is_connected) {
                        const char *mac = get_device_mac_str();
                        char status_topic[128];
                        snprintf(status_topic, sizeof(status_topic),
                                 "device/%s/status", mac);

                        SIM7600_mqtt_publish(status_topic, "online");
                        esp_err_t pub_ret = SIM7600_mqtt_publish_data(
                                cmd.data.publish.topic, cmd.data.publish.payload);
                        memset(cmd.data.publish.payload, 0, sizeof(cmd.data.publish.payload));

                        if (pub_ret != ESP_OK) {
                            int rssi = get_signal_quality();
                            ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                     "Publish failed — RSSI: %d", rssi);

                            if (rssi == -1) {
                                /* No signal: payload already saved to SD inside
                                 * SIM7600_mqtt_publish_data. Count consecutive hits. */
                                mqtt_ctx.no_signal_fail_count++;
                                ESP_LOGW(TAG_MODEM_MQTT_HANDLER,
                                         "No signal — SD save done (%d/3)",
                                         mqtt_ctx.no_signal_fail_count);

                                if (mqtt_ctx.no_signal_fail_count >= 3) {
                                    ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                             "3 consecutive no-signal failures — "
                                             "reconnecting data + MQTT");
                                    mqtt_ctx.no_signal_fail_count = 0;
                                    mqtt_ctx.is_connected = false;
                                    connected = false;
                                    SIM7600_mqtt_disconnect();
                                    modem_connect_data();
                                    /* Re-queue a full MQTT reconnect */
                                    mqtt_command_t reconnect_cmd = { .cmd = MQTT_CMD_CONNECT };
                                    xQueueSend(mqtt_ctx.command_queue, &reconnect_cmd, 0);
                                }
                            } else {
                                /* Signal is present but MQTT session is broken
                                 * (e.g. +CMQTTTOPIC error 11 = modem MQTT dropped).
                                 * No point retrying — reconnect MQTT immediately. */
                                mqtt_ctx.no_signal_fail_count = 0;
                                ESP_LOGW(TAG_MODEM_MQTT_HANDLER,
                                         "Signal OK but MQTT broken — reconnecting MQTT");
                                mqtt_ctx.is_connected = false;
                                SIM7600_mqtt_disconnect();
                                mqtt_command_t reconnect_cmd = { .cmd = MQTT_CMD_CONNECT };
                                xQueueSend(mqtt_ctx.command_queue, &reconnect_cmd, 0);
                            }
                        } else {
                            /* Successful publish — reset counter */
                            mqtt_ctx.no_signal_fail_count = 0;
                            /* Drain any payloads saved during a previous signal
                             * loss (flag set without a full MQTT disconnect) */
                            if (g_sd_has_pending_payloads) {
                                ESP_LOGI(TAG_MODEM_MQTT_HANDLER,
                                         "SD has pending payloads — syncing via HTTP");
                                http_sync_pending_payloads(get_device_mac_str());
                            }
                        }
                    }
                    break;
                 case MQTT_CMD_PUBLISH_LOGS:
                    if (mqtt_ctx.is_connected) {


                            const char *mac = get_device_mac_str();
                            char status_topic[128];

                            snprintf(status_topic, sizeof(status_topic),
                                     "device/%s/status", mac);
                            ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Publish online !");
                            SIM7600_mqtt_publish(status_topic, "online");
                            SIM7600_mqtt_publish_log(cmd.data.publish.topic, cmd.data.publish.payload);
                            memset(cmd.data.publish.payload, 0, sizeof(cmd.data.publish.payload));
                    }
                    break;
                    
                case MQTT_CMD_SUBSCRIBE:
                    if (mqtt_ctx.is_connected) {
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Subscribing to: %s", cmd.data.subscribe.topic);
                       
                        SIM7600_mqtt_subscribe(cmd.data.subscribe.topic);
                        // Post subscribed event
                        esp_event_post(MQTT_EVENTS, MQTT_EVENT_SUBSCRIBED, NULL, 0, portMAX_DELAY);
                    } else {
                        ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Cannot subscribe - MQTT not connected");
                    }
                    break;
                    
                case MQTT_CMD_UNSUBSCRIBE:
                    if (mqtt_ctx.is_connected) {
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Unsubscribing from: %s", cmd.data.unsubscribe.topic);
                       
                        SIM7600_mqtt_unsubscribe(cmd.data.unsubscribe.topic);
                    }
                    break;
                    
                case MQTT_CMD_RECONNECT:
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Attempting to reconnect (attempt %d)", 
                             mqtt_ctx.reconnect_attempts + 1);
                    
                    // Queue connect command
                    mqtt_command_t connect_cmd = {
                        .cmd = MQTT_CMD_CONNECT
                    };
                    xQueueSend(mqtt_ctx.command_queue, &connect_cmd, 0);
                    break;
                
                case MQTT_CMD_READY:
                         mqtt_ctx.reconnect_attempts=0; // Reset attempts on manual connec
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "  MQTT ready ");
                        if (mqtt_main_task_handler == NULL) {
                            xTaskCreate(mqtt_main_task,"mqtt_pub_task", 12288,NULL,1, &mqtt_main_task_handler);
                        } else {
                            vTaskResume(mqtt_main_task_handler); // Suspend MQTT task before modem restart
                        }

                    break;
                case MQTT_CMD_MESSAGE_RECEIVED: {
                    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Processing received MQTT message");
                    
                    // Check if pointers are valid
                    if (cmd.data.receive_msg.topic && cmd.data.receive_msg.payload) {
                        ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "MQTT Message: %s -> %s", 
                                cmd.data.receive_msg.topic, cmd.data.receive_msg.payload);
                        
                        // Handle commands
                        if (strcmp(cmd.data.receive_msg.topic, "esp32/sim7000/commands") == 0) {
                            if (strcmp(cmd.data.receive_msg.payload, "get_gps") == 0) {
                                ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Get GPS command received");
                                // modem_get_gps();
                            } else if (strcmp(cmd.data.receive_msg.payload, "restart") == 0) {
                                ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Restart command received");
                                // Handle restart
                            } else {
                                ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Unknown command: %s", 
                                        cmd.data.receive_msg.payload);
                            }
                        } else {
                            ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Message on non-command topic: %s", 
                                    cmd.data.receive_msg.topic);
                        }
                    } else {
                        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Invalid message data received");
                    }
                    
                    // Free the allocated memory after processing
                    if (cmd.data.receive_msg.topic) {
                        free((void*)cmd.data.receive_msg.topic);
                    }
                    if (cmd.data.receive_msg.payload) {
                        free((void*)cmd.data.receive_msg.payload);
                    }
                    break;
                } 
                case CMD_OTA:
                    ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Attempting to start OTA: %s",
                             cmd.data.ota.ota_url[0] ? cmd.data.ota.ota_url : "(no url)");
                    if (cmd.data.ota.ota_url[0] == '\0') {
                        ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "OTA aborted: no URL provided");
                        break;
                    }
                    mqtt_main_task_stop();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    download_firmware_safe(cmd.data.ota.ota_url);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    mqtt_main_task_start();
                    break;
                case UPDATE_CONFIG:
                    ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Applying device config update");
                    if (cmd.data.update_config.config_json[0] != '\0') {
                        esp_err_t cfg_ret = device_config_apply_from_json(
                            cmd.data.update_config.config_json);
                        if (cfg_ret == ESP_OK) {
                            ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Config updated OK");
                        } else {
                            ESP_LOGE(TAG_MODEM_MQTT_HANDLER,
                                     "Config update failed: %s", esp_err_to_name(cfg_ret));
                        }
                    } else {
                        ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "UPDATE_CONFIG: empty payload");
                    }
                    break;
                default:
                    ESP_LOGW(TAG_MODEM_MQTT_HANDLER, "Unknown MQTT command: %d", cmd.cmd);
                    break;
            }
        }
    }
}

/**
 * @brief Reconnect timer callback
 */
static void mqtt_reconnect_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Reconnect timer triggered");
    
    // Queue reconnect command
    mqtt_command_t cmd = {
        .cmd = MQTT_CMD_RECONNECT
    };
    
    xQueueSend(mqtt_ctx.command_queue, &cmd, 0);
}


/**
 * @brief MQTT message received callback
 */
static void mqtt_message_callback(const char *topic, const char *payload) {
    // ESP_LOGI(TAG_MODEM_MQTT_HANDLER, "Received message - Topic: %s, Payload: %s", topic, payload);
    
    // Create event data
    mqtt_message_event_t *event_data = malloc(sizeof(mqtt_message_event_t));
    if (event_data) {
        // Copy strings with null termination
        strncpy(event_data->topic, topic, sizeof(event_data->topic) - 1);
        event_data->topic[sizeof(event_data->topic) - 1] = '\0';
        strncpy(event_data->payload, payload, sizeof(event_data->payload) - 1);
        event_data->payload[sizeof(event_data->payload) - 1] = '\0';
        
        // Post message received event
        esp_event_post(MQTT_EVENTS, MQTT_EVENT_MESSAGE_RECEIVED,
                      event_data, sizeof(mqtt_message_event_t), portMAX_DELAY);
        
        // Create command with dynamically allocated strings
        mqtt_command_t cmd;
        cmd.cmd = MQTT_CMD_MESSAGE_RECEIVED;
        
        // Allocate and copy topic
        cmd.data.receive_msg.topic = malloc(strlen(event_data->topic) + 1);
        if (cmd.data.receive_msg.topic) {
            strcpy((char*)cmd.data.receive_msg.topic, event_data->topic);
        } else {
            ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to allocate memory for topic");
            free(event_data);
            return;
        }
        
        // Allocate and copy payload
        cmd.data.receive_msg.payload = malloc(strlen(event_data->payload) + 1);
        if (cmd.data.receive_msg.payload) {
            strcpy((char*)cmd.data.receive_msg.payload, event_data->payload);
        } else {
            ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to allocate memory for payload");
            free((void*)cmd.data.receive_msg.topic);
            free(event_data);
            return;
        }
        
        // Send to queue
        if (xQueueSend(mqtt_ctx.command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(TAG_MODEM_MQTT_HANDLER, "Failed to queue message command");
            // Free allocated memory if queue send fails
            free((void*)cmd.data.receive_msg.topic);
            free((void*)cmd.data.receive_msg.payload);
        }
        
        free(event_data);
    }
}

