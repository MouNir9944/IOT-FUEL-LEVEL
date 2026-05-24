#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "Commun_lib.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_mac.h"       /* esp_read_mac() — works without WiFi init */
#include "nvs.h"
#include <time.h>
#include "SIM7000_MQTT.h"  /* sim7000_mqtt_publish() for modem path    */

/* ── Broker defaults (overridden by Config.txt: mqtt_broker/mqtt_user/mqtt_pass) ── */
#define MQTT_DEFAULT_BROKER   "mqtt://broker.hivemq.com:1883"
#define MQTT_DEFAULT_USERNAME ""
#define MQTT_DEFAULT_PASSWORD ""

/* ── Topic pattern: WATER/<device_id>/<suffix> ────────────────────────────── */
#define TOPIC(buf, suffix) \
    char buf[72]; snprintf(buf, sizeof(buf), "WATER/%s/" suffix, device_id)

static const char *TAG_MQTT = "MQTT";
#define MQTT_RECONNECT_DELAY pdMS_TO_TICKS(5000)

extern esp_mqtt_client_handle_t client;

/* ── Device ID (set once from MAC in mqtt_app_start) ─────────────────────── */
char device_id[14] = {0};

/* ── Control mode ("auto" = plan scheduler drives valve, "manual" = MQTT cmd) */
char control_mode[8] = "manual";

/* ── Timezone offset from UTC in minutes ─────────────────────────────────── */
int g_utc_offset_min = 0;

/* ── Task handles ─────────────────────────────────────────────────────────── */
static TaskHandle_t telemetry_task_handle = NULL;

/* ── Device-ID initialisation — reads eFuse MAC (WiFi + modem modes) ─────── */
static void init_device_id(void)
{
    if (device_id[0] != '\0') return;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device_id, sizeof(device_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG_MQTT, "Device ID: %s", device_id);
}

/* ── Unified publish — routes to WiFi MQTT client or SIM7000 modem ───────── */
static int mqtt_publish_any(const char *topic, const char *payload)
{
    if (strcmp(g_conn_mode, "modem") == 0)
        return (sim7000_mqtt_publish(topic, payload) == ESP_OK) ? 0 : -1;
    if (!client) return -1;
    return esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);
}

/* ── Immediate telemetry push ─────────────────────────────────────────────── */
static inline void telemetry_push_now(void)
{
    if (telemetry_task_handle)
        xTaskNotify(telemetry_task_handle, 0, eNoAction);
}

/* ── Forward declaration (body in flow_meter.h, included below) ───────────── */
void read_telemetry_task(void *pvParameter);

/* ── Pull in plan manager first (flow_meter.h needs plan_active_slice_t) ──── */
#include "plan_manager.h"

/* ── Helpers (defined before flow_meter.h which calls them) ─────────────── */

static void get_iso8601_time(char *buf, size_t len)
{
    time_t now = 0;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);   /* uses TZ env var set by set_esp32_timezone() */

    /* Format base: 2026-05-24T10:17:47 */
    char base[24];
    strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &t);

    /* Append UTC offset from g_utc_offset_min: e.g. +01:00 or -05:30 */
    int off  = g_utc_offset_min;
    char sgn = (off >= 0) ? '+' : '-';
    if (off < 0) off = -off;
    snprintf(buf, len, "%s%c%02d:%02d", base, sgn, off / 60, off % 60);
}

static uint32_t get_uptime_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

/* ── Pull in flow meter + telemetry task ─────────────────────────────────── */
#include "flow_meter.h"

/* ── Pull in OTA manager ─────────────────────────────────────────────────── */
#include "ota_manager.h"

/* ── MQTT command dispatcher ─────────────────────────────────────────────── *
 * Called by the WiFi MQTT event handler AND by the SIM7000 MQTT callback.   *
 * topic_buf / data_buf are NUL-terminated strings.                          */
static void handle_mqtt_command(const char *topic_buf, const char *data_buf)
{
    TOPIC(expected_cmd, "cmd");
    if (strcmp(topic_buf, expected_cmd) != 0) return;

    cJSON *root = cJSON_Parse(data_buf);
    if (!root) { ESP_LOGW(TAG_MQTT, "Bad JSON"); return; }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd_item)) { cJSON_Delete(root); return; }

    const char *cmd = cmd_item->valuestring;
    ESP_LOGI(TAG_MQTT, "Command: %s", cmd);

    /* ── set_plan (legacy single-message path) ───────────────── */
    if (strcmp(cmd, "set_plan") == 0) {
        cJSON *plan_obj = cJSON_GetObjectItem(root, "plan");
        if (plan_obj) {
            plan_handle_set(plan_obj);
            plan_immediate_apply();
        }
        telemetry_push_now();
        cJSON_Delete(root);
        return;
    }

    /* ── plan_start / plan_rule / plan_apply ─────────────────── *
     * Multi-message plan delivery for large plans that exceed    *
     * the SIM7000's 512-byte SMSUB limit.  Backend sends:        *
     *   plan_start → plan_rule × N → plan_apply                 */
    if (strcmp(cmd, "plan_start") == 0) {
        plan_handle_start(root);   /* root carries id/n/en/ds/de */
        cJSON_Delete(root);
        return;
    }
    if (strcmp(cmd, "plan_rule") == 0) {
        plan_handle_rule(root);    /* root carries id/d/s */
        cJSON_Delete(root);
        return;
    }
    if (strcmp(cmd, "plan_apply") == 0) {
        cJSON *pid_item = cJSON_GetObjectItem(root, "id");
        plan_handle_apply(cJSON_IsString(pid_item) ? pid_item->valuestring : NULL);
        plan_immediate_apply();
        telemetry_push_now();
        cJSON_Delete(root);
        return;
    }

    /* ── delete_plan ──────────────────────────────────────────── */
    if (strcmp(cmd, "delete_plan") == 0) {
        cJSON *pid_item = cJSON_GetObjectItem(root, "plan_id");
        if (cJSON_IsString(pid_item))
            plan_handle_delete(pid_item->valuestring);
        telemetry_push_now();
        cJSON_Delete(root);
        return;
    }

    /* ── ota_update ───────────────────────────────────────────── */
    if (strcmp(cmd, "ota_update") == 0) {
        cJSON *url_item = cJSON_GetObjectItem(root, "url");
        if (cJSON_IsString(url_item) && url_item->valuestring[0]) {
            strlcpy(ota_url_g, url_item->valuestring, sizeof(ota_url_g));
            xTaskCreate(ota_update_task, "ota", 16384, ota_url_g, 5, NULL);
        }
        cJSON_Delete(root);
        return;
    }

    /* ── reset_flow: zero the total volume counter ────────────── */
    if (strcmp(cmd, "reset_flow") == 0) {
        flow_meter_reset();
        telemetry_push_now();
        cJSON_Delete(root);
        return;
    }

    /* ── Commands that need a 'value' field ───────────────────── */
    cJSON *val_item = cJSON_GetObjectItem(root, "value");
    if (!val_item) {
        ESP_LOGW(TAG_MQTT, "Missing 'value' for cmd: %s", cmd);
        cJSON_Delete(root);
        return;
    }

    /* ── set_valve: "OPEN" | "CLOSE" | "STOP" ────────────────── */
    if (strcmp(cmd, "set_valve") == 0 && cJSON_IsString(val_item)) {
        const char *val = val_item->valuestring;
        if      (strcmp(val, "OPEN")  == 0) { valve_open();  led_valve_set(1); }
        else if (strcmp(val, "CLOSE") == 0) { valve_close(); led_valve_set(0); }
        else if (strcmp(val, "STOP")  == 0) { valve_stop();  led_valve_set(0); }
        else ESP_LOGW(TAG_MQTT, "set_valve: unknown value '%s'", val);

    /* ── set_control_mode: "auto" | "manual" ─────────────────── */
    } else if (strcmp(cmd, "set_control_mode") == 0 && cJSON_IsString(val_item)) {
        const char *val = val_item->valuestring;
        if (strcmp(val, "auto") == 0 || strcmp(val, "manual") == 0) {
            strlcpy(control_mode, val, sizeof(control_mode));
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "ctrl_mode", control_mode);
                nvs_commit(h); nvs_close(h);
            }
            led_auto_set(strcmp(val, "auto") == 0);
            if (strcmp(val, "auto") == 0) {
                plan_sched_ensure_running();
                plan_immediate_apply();
            } else {
                valve_stop(); led_valve_set(0);
            }
            ESP_LOGI(TAG_MQTT, "Control mode → %s (saved)", val);
        }
    } else {
        ESP_LOGW(TAG_MQTT, "Unknown command: %s", cmd);
    }

    telemetry_push_now();
    cJSON_Delete(root);
}

/* ── Timezone helpers ────────────────────────────────────────────────────── */

static void _apply_timezone(int utc_offset_min)
{
    int posix_off_min = -utc_offset_min;
    int abs_h = abs(posix_off_min) / 60;
    int abs_m = abs(posix_off_min) % 60;
    char tz[24];

    if (abs_m == 0) {
        snprintf(tz, sizeof(tz),
                 posix_off_min < 0 ? "UTC-%d" : "UTC+%d", abs_h);
    } else {
        snprintf(tz, sizeof(tz),
                 posix_off_min < 0 ? "UTC-%d:%02d" : "UTC+%d:%02d",
                 abs_h, abs_m);
    }
    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG_MQTT, "Timezone → %s  (UTC%+d min)", tz, utc_offset_min);
}

static void _sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    _apply_timezone(g_utc_offset_min);

    time_t now;
    time(&now);
    struct tm local_tm;
    localtime_r(&now, &local_tm);

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &local_tm);
    ESP_LOGI(TAG_MQTT, "SNTP synced — local: %s  (UTC%+d min)", ts, g_utc_offset_min);

    strlcpy(g_time_source, "ntp", sizeof(g_time_source));

    rtc_ds1307_sync_from_system();

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i64(h, "ntp_epoch", (int64_t)now);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* ── Online status ───────────────────────────────────────────────────────── */
static void publish_status_online(esp_mqtt_client_handle_t c)
{
    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    char ts[32];
    get_iso8601_time(ts, sizeof(ts));

    TOPIC(topic, "status");
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"device_id\":\"%s\","
         "\"status\":\"online\","
         "\"last_seen\":\"%s\","
         "\"rssi\":%d,"
         "\"uptime_seconds\":%lu,"
         "\"sw_version\":\"" FW_SW_VERSION "\","
         "\"hw_version\":\"" FW_HW_VERSION "\"}",
        device_id, ts, rssi, (unsigned long)get_uptime_s());

    esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG_MQTT, "Status published  rssi=%d  uptime=%lu s",
             rssi, (unsigned long)get_uptime_s());
}

/* ── MQTT event handler ──────────────────────────────────────────────────── */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {

        case MQTT_EVENT_CONNECTED:
            led_mqtt_set(1);
            ESP_LOGI(TAG_MQTT, "Connected");

            /* Re-seed clock from DS1307 if it drifted stale while offline */
            {
                time_t cur;
                time(&cur);
                if ((unsigned long)cur < MIN_VALID_EPOCH) {
                    rtc_ds1307_seed_system();
                }
                if (!esp_sntp_enabled()) {
                    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                    esp_sntp_setservername(0, "pool.ntp.org");
                    esp_sntp_set_time_sync_notification_cb(_sntp_sync_cb);
                    esp_sntp_init();
                }
            }

            /* Subscribe to command topic */
            {
                TOPIC(cmd_topic, "cmd");
                esp_mqtt_client_subscribe(event->client, cmd_topic, 0);
                ESP_LOGI(TAG_MQTT, "Subscribed → %s", cmd_topic);
            }

            publish_status_online(event->client);

            if (telemetry_task_handle == NULL)
                xTaskCreate(read_telemetry_task, "telemetry", 4096,
                            NULL, 5, &telemetry_task_handle);
            break;

        case MQTT_EVENT_DISCONNECTED:
            led_mqtt_set(0);
            ESP_LOGI(TAG_MQTT, "Disconnected");

            if (telemetry_task_handle != NULL) {
                vTaskDelete(telemetry_task_handle);
                telemetry_task_handle = NULL;
            }

            {
                wifi_ap_record_t _ap;
                if (esp_wifi_sta_get_ap_info(&_ap) == ESP_OK) {
                    ESP_LOGI(TAG_MQTT,
                             "WiFi still up (SSID:%s RSSI:%d) — reconnecting in 5 s",
                             (char *)_ap.ssid, _ap.rssi);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    esp_mqtt_client_reconnect(event->client);
                } else {
                    ESP_LOGI(TAG_MQTT, "WiFi also down — waiting for STA reconnect");
                }
            }
            break;

        case MQTT_EVENT_DATA: {
                /* Skip retained broker replays */
                if (event->retain) {
                    ESP_LOGW(TAG_MQTT, "Ignoring retained msg — skipped");
                    break;
                }

                char topic_buf[72] = {0};
                int tlen = event->topic_len < (int)sizeof(topic_buf) - 1
                            ? event->topic_len : (int)sizeof(topic_buf) - 1;
                memcpy(topic_buf, event->topic, tlen);

                int dlen = event->data_len;
                char *data_buf = (char *)malloc(dlen + 1);
                if (!data_buf) {
                    ESP_LOGE(TAG_MQTT, "OOM: cannot allocate %d bytes", dlen + 1);
                    break;
                }
                memcpy(data_buf, event->data, dlen);
                data_buf[dlen] = '\0';

                ESP_LOGI(TAG_MQTT, "DATA  topic=%s  data=%s", topic_buf, data_buf);
                handle_mqtt_command(topic_buf, data_buf);
                free(data_buf);
                break;
            }

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_MQTT, "Subscribed  msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG_MQTT, "Published   msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG_MQTT, "Error");
            break;
        default:
            break;
    }
}

/* ── mqtt_app_start — WiFi path only ─────────────────────────────────────── */
void mqtt_app_start(esp_mqtt_client_handle_t *c)
{
    /* In modem mode the modem_app task handles connectivity; skip this. */
    if (strcmp(g_conn_mode, "modem") == 0) {
        ESP_LOGI(TAG_MQTT, "Modem mode — skipping WiFi MQTT start");
        return;
    }

    led_auto_set(strcmp(control_mode, "auto") == 0);
    init_device_id();

    const char *broker   = MQTT_DEFAULT_BROKER;
    const char *username = MQTT_DEFAULT_USERNAME;
    const char *password = MQTT_DEFAULT_PASSWORD;

    char *raw = read_data("/spiffs/Config.txt");
    cJSON *cfg = (raw && raw[0]) ? cJSON_Parse(raw) : NULL;
    free(raw);

    char broker_buf[80]   = {0};
    char username_buf[32] = {0};
    char password_buf[32] = {0};

    if (cfg) {
        cJSON *b = cJSON_GetObjectItem(cfg, "mqtt_broker");
        cJSON *u = cJSON_GetObjectItem(cfg, "mqtt_user");
        cJSON *p = cJSON_GetObjectItem(cfg, "mqtt_pass");
        if (cJSON_IsString(b) && b->valuestring[0]) {
            strlcpy(broker_buf,   b->valuestring, sizeof(broker_buf));
            broker = broker_buf;
        }
        if (cJSON_IsString(u) && u->valuestring[0]) {
            strlcpy(username_buf, u->valuestring, sizeof(username_buf));
            username = username_buf;
        }
        if (cJSON_IsString(p) && p->valuestring[0]) {
            strlcpy(password_buf, p->valuestring, sizeof(password_buf));
            password = password_buf;
        }
        cJSON *tz_cfg = cJSON_GetObjectItem(cfg, "utc_offset_minutes");
        if (cJSON_IsNumber(tz_cfg))
            g_utc_offset_min = (int)tz_cfg->valuedouble;
        cJSON_Delete(cfg);
    }

    _apply_timezone(g_utc_offset_min);

    static char lwt_topic[72];
    static char lwt_payload[128];
    snprintf(lwt_topic,   sizeof(lwt_topic),
             "WATER/%s/status/lwt", device_id);
    snprintf(lwt_payload, sizeof(lwt_payload),
        "{\"device_id\":\"%s\",\"status\":\"offline\","
         "\"reason\":\"unexpected_disconnect\"}", device_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri                   = broker,
        .credentials.username                 = username,
        .credentials.authentication.password  = password,
        .session.last_will.topic              = lwt_topic,
        .session.last_will.msg                = lwt_payload,
        .session.last_will.msg_len            = strlen(lwt_payload),
        .session.last_will.qos                = 1,
        .session.last_will.retain             = 0,
        .session.keepalive                    = 10,
        .buffer.size                          = 4096,
        .buffer.out_size                      = 4096,
    };

    ESP_LOGI(TAG_MQTT, "Connecting to %s  user=%s  device=%s",
             broker, username, device_id);

    *c = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(*c, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(*c);
}

#endif /* MQTT_MANAGER_H */
