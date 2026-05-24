#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "commun_lib.h"
#include "config_mode.h"
#include "mqtt_manager.h"
#include "esp_wifi.h"

extern httpd_handle_t server;
extern esp_mqtt_client_handle_t client;

#define DEFAULT_SCAN_LIST_SIZE  20
#define WIFI_SSID_AP            "Droppy-Setup"
#define WIFI_PASS_AP            "Droppy1234"
#define MAX_STA_CONN            1
#define AP_TIMEOUT_MS           120000

#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5]
#endif

static const char *TAG_WIFI    = "wifi";
static volatile bool wifi_scan_in_progress = false;

/* Set true by wifi_init_all() (WiFi mode) or by the lazy AP init inside
 * wifi_enable_ap() (modem mode).  Guards every esp_wifi_* call so they are
 * never issued against an uninitialised driver.                             */
static bool g_wifi_initialized = false;

/* AP auto-close timer */
static TimerHandle_t ap_timer_h = NULL;

/* Forward declarations */
void wifi_enable_ap(void);
void wifi_disable_ap(void);
static void ap_timer_cb(TimerHandle_t xTimer);

/* Forward declarations for mqtt_manager.h functions used in wifi event handler.
 * Bodies are defined later in the single translation unit (mqtt_manager.h). */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data);
void mqtt_app_start(esp_mqtt_client_handle_t *c);

/* ── AP timeout callback ─────────────────────────────────────────────────── */
static void ap_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG_WIFI, "AP timeout — no client, closing AP");
    wifi_disable_ap();
}

/* ── WiFi event handler ──────────────────────────────────────────────────── */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base != WIFI_EVENT) return;
    switch (event_id) {

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *e = event_data;
            ESP_LOGI(TAG_WIFI, "AP client joined " MACSTR, MAC2STR(e->mac));
            if (ap_timer_h) {
                xTimerStop(ap_timer_h, 0);
                xTimerDelete(ap_timer_h, 0);
                ap_timer_h = NULL;
            }
            if (server == NULL) server = start_webserver();
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *e = event_data;
            ESP_LOGI(TAG_WIFI, "AP client left " MACSTR, MAC2STR(e->mac));
            if (server) { stop_webserver(server); server = NULL; }
            wifi_disable_ap();
            break;
        }

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG_WIFI, "STA start — connecting");
            if (!wifi_scan_in_progress) {
                esp_wifi_connect();
            } else {
                ESP_LOGI(TAG_WIFI, "STA connect paused: scan in progress");
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            led_wifi_set(0);   /* WiFi lost */
            led_mqtt_set(0);   /* MQTT is also gone when WiFi drops */
            /* In modem mode the STA is never used — do not retry */
            if (!wifi_test_mode && strcmp(g_conn_mode, "modem") != 0) {
                if (client) {
                    esp_mqtt_client_stop(client);
                    esp_mqtt_client_disconnect(client);
                    esp_mqtt_client_unregister_event(client, ESP_EVENT_ANY_ID,
                                                     mqtt_event_handler);
                    client = NULL;
                }
                if (!wifi_scan_in_progress) {
                    ESP_LOGI(TAG_WIFI, "STA disconnected — retrying");
                    esp_wifi_connect();
                } else {
                    ESP_LOGI(TAG_WIFI, "STA reconnect paused for scan");
                }
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            led_wifi_set(1);   /* WiFi associated */
            ESP_LOGI(TAG_WIFI, "STA connected");
            if (!wifi_test_mode) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                mqtt_app_start(&client);
                esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                               mqtt_event_handler, client);
            }
            break;

        default: break;
    }
}

/* ── WiFi scan ───────────────────────────────────────────────────────────── */
cJSON *wifi_scan_ap(void)
{
    cJSON *root     = cJSON_CreateObject();
    cJSON *ap_array = cJSON_CreateArray();
    cJSON_AddNumberToObject(root, "state", 1);

    uint16_t number   = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t  ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    wifi_scan_config_t scan_config = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = true,
    };

    bool scan_started = false;
    wifi_scan_in_progress = true;
    /* Pause STA reconnect loops while scanning. */
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        if (err == ESP_ERR_WIFI_STATE) {
            ESP_LOGW(TAG_WIFI, "WiFi scan skipped: STA is busy connecting");
        } else {
            ESP_LOGE(TAG_WIFI, "WiFi scan failed: %s", esp_err_to_name(err));
        }
        wifi_scan_in_progress = false;
        if (!wifi_test_mode) esp_wifi_connect();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
        cJSON_AddItemToObject(root, "wifi_networks", ap_array);
        return root;
    } else {
        scan_started = true;
        esp_wifi_scan_get_ap_num(&ap_count);
        esp_wifi_scan_get_ap_records(&number, ap_info);
        cJSON_AddBoolToObject(root, "success", true);
    }

    cJSON *cnt = cJSON_CreateObject();
    cJSON_AddNumberToObject(cnt, "n_station", ap_count);
    cJSON_AddItemToArray(ap_array, cnt);

    for (int i = 0; i < DEFAULT_SCAN_LIST_SIZE && i < ap_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddStringToObject(item, "state_wifi",
            ap_info[i].rssi > -50 ? "Good" :
            ap_info[i].rssi > -70 ? "Ok"   : "Weak");
        cJSON_AddStringToObject(item, "Band",
            ap_info[i].primary < 14 ? "2.4GHz" : "5GHz");
        cJSON_AddItemToArray(ap_array, item);
    }

    cJSON_AddItemToObject(root, "wifi_networks", ap_array);
    if (scan_started) {
        esp_wifi_clear_ap_list();
    }
    wifi_scan_in_progress = false;
    if (!wifi_test_mode) esp_wifi_connect();
    return root;
}

/* ── wifi_init_all ───────────────────────────────────────────────────────── */
void wifi_init_all(const char *ssid, const char *password)
{
    /* led_init() is called in app_main() before this function so LED GPIOs
     * are already configured.  Do NOT call it here — it would reset every
     * LED to OFF, overwriting the Auto LED state set by plan_manager_init. */

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();   /* create AP netif now so mode switch works later */

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);

    wifi_config_t sta_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e        = WPA3_SAE_PWE_BOTH,
        }
    };
    if (ssid && ssid[0])
        strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    if (password && password[0])
        strlcpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);

    /* Disable modem sleep so the WiFi ISR cannot steal the CPU for hundreds
     * of microseconds mid-measurement and corrupt the DHT22 1-wire protocol.
     * WIFI_PS_NONE keeps the radio always-on; the power cost (~20 mA extra)
     * is acceptable for a mains-powered AC controller.                        */
    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_wifi_start();
    /* WIFI_EVENT_STA_START → esp_wifi_connect() called in handler */
    g_wifi_initialized = true;

    ESP_LOGI(TAG_WIFI, "STA started  ssid=%s",
             (ssid && ssid[0]) ? ssid : "(none — press button to configure)");
}

/* ── wifi_enable_ap ──────────────────────────────────────────────────────── */
void wifi_enable_ap(void)
{
    if (!g_wifi_initialized) {
        /* ── Modem mode: WiFi driver was never started.
         * Do a minimal one-shot init for AP-only operation so the setup page
         * can be served over WiFi while the modem runs on UART independently.
         * STA is not created — no credentials, no reconnect loop.            */
        ESP_LOGI(TAG_WIFI, "Lazy WiFi init for setup AP (modem mode)");
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&wcfg);
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_event_handler, NULL, NULL);
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_start();
        g_wifi_initialized = true;
    } else if (strcmp(g_conn_mode, "modem") != 0) {
        /* WiFi mode: STA is already running — add AP alongside it */
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    /* In modem mode with WiFi already lazily init'd: already in WIFI_MODE_AP */

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len       = strlen(WIFI_SSID_AP),
            .max_connection = MAX_STA_CONN,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        }
    };
    strlcpy((char *)ap_cfg.ap.ssid,     WIFI_SSID_AP, sizeof(ap_cfg.ap.ssid));
    strlcpy((char *)ap_cfg.ap.password, WIFI_PASS_AP,  sizeof(ap_cfg.ap.password));

    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);

    ESP_LOGI(TAG_WIFI, "AP enabled  SSID:%s  PASS:%s  (%d s timeout)",
             WIFI_SSID_AP, WIFI_PASS_AP, AP_TIMEOUT_MS / 1000);

    if (ap_timer_h) {
        xTimerReset(ap_timer_h, 0);                          /* already running — reset */
    } else {
        ap_timer_h = xTimerCreate("ap_tmr", pdMS_TO_TICKS(AP_TIMEOUT_MS),
                                  pdFALSE, NULL, ap_timer_cb);
        xTimerStart(ap_timer_h, 0);
    }
}

/* ── wifi_disable_ap ─────────────────────────────────────────────────────── */
void wifi_disable_ap(void)
{
    if (ap_timer_h) {
        xTimerStop(ap_timer_h, 0);
        xTimerDelete(ap_timer_h, 0);
        ap_timer_h = NULL;
    }
    if (server) { stop_webserver(server); server = NULL; }
    if (strcmp(g_conn_mode, "modem") == 0) {
        /* Modem mode — shut WiFi off completely; modem handles connectivity */
        esp_wifi_set_mode(WIFI_MODE_NULL);
        ESP_LOGI(TAG_WIFI, "AP closed — WiFi off (modem mode)");
    } else {
        /* WiFi mode — drop back to STA-only so normal connectivity resumes */
        esp_wifi_set_mode(WIFI_MODE_STA);
        ESP_LOGI(TAG_WIFI, "AP closed — STA-only mode");
    }
}

/* ── Helpers used by config_mode.h ──────────────────────────────────────── */
esp_err_t setup_wifi_connect(char *_ssid, char *_password)
{
    wifi_config_t cfg = { .sta = { .bssid_set = false } };
    strlcpy((char *)cfg.sta.ssid,     _ssid,     sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, _password, sizeof(cfg.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    return esp_wifi_connect();
}

char *get_ap_info(void)
{
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return NULL;
    char *buf = malloc(96);
    if (!buf) return NULL;
    snprintf(buf, 96, "SSID:%s  RSSI:%d dBm  CH:%d",
             info.ssid, info.rssi, info.primary);
    return buf;
}

void clear_sta_config(void)
{
    wifi_config_t empty = { .sta = { .ssid = "", .password = "" } };
    esp_wifi_set_config(WIFI_IF_STA, &empty);
}

void disconnect_ST(void) { esp_wifi_disconnect(); }

/* ── Called by the setup button before enabling AP ──────────────────────── */
/*
 * If STA is currently connected → do nothing (keep the link alive, AP will
 * run alongside in APSTA mode).
 * If STA is NOT connected (in the retry loop) → stop the retry so the loop
 * no longer fights with AP setup.  We reuse wifi_scan_in_progress to tell
 * the WIFI_EVENT_STA_DISCONNECTED handler not to call esp_wifi_connect().
 */
void wifi_stop_sta_if_not_connected(void)
{
    if (!g_wifi_initialized) {
        /* Modem mode: WiFi not yet running — nothing to stop */
        ESP_LOGI(TAG_WIFI, "AP button: WiFi not yet init (modem mode) — skipping STA stop");
        return;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        /* Already associated — log it and leave STA alone */
        ESP_LOGI(TAG_WIFI, "AP button: STA connected (%s, %d dBm) — keeping link",
                 (char *)ap_info.ssid, ap_info.rssi);
        return;
    }

    /* Not connected → stop the retry loop before switching to APSTA */
    ESP_LOGI(TAG_WIFI, "AP button: STA not connected — stopping retry loop");
    wifi_scan_in_progress = true;   /* suppress reconnect in event handler */
    esp_wifi_disconnect();          /* abort the current attempt */
    vTaskDelay(pdMS_TO_TICKS(400)); /* let DISCONNECTED event fire and settle */
    wifi_scan_in_progress = false;
}

#endif /* WIFI_MANAGER_H */
