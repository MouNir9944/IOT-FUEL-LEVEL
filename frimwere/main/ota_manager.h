#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

/* ── Includes ────────────────────────────────────────────────────────────────
 * This file is included from within mqtt_manager.h, so the following symbols
 * are already in scope:
 *   client                 – esp_mqtt_client_handle_t (extern, from mqtt_manager.h)
 *   device_id[]            – char[14], e.g. "AABBCCDDEEFF"
 *   telemetry_task_handle  – TaskHandle_t
 *
 * Uses esp_https_ota (advanced API) instead of raw esp_http_client so that
 * the TLS handshake runs through Espressif's tested HTTPS-OTA path, which
 * correctly handles:
 *   • the mbedTLS SSL context setup (avoids -0x7780 handshake failures)
 *   • CA-bundle verification via esp_crt_bundle_attach
 *   • SNI for virtual-hosted HTTPS (Render.com / Cloudflare)
 *   • Progress streaming with esp_https_ota_perform chunks
 */
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

/* URL written by the mqtt_event_handler before spawning ota_update_task */
static char ota_url_g[512] = {0};

/* ── Internal helpers ────────────────────────────────────────────────────── */

/** Publish a JSON string to WATER/<device_id>/ota/<suffix>. */
static void _ota_pub(const char *suffix, const char *json)
{
    char topic[80];
    snprintf(topic, sizeof(topic), "WATER/%s/ota/%s", device_id, suffix);
    esp_mqtt_client_publish(client, topic, json, 0, 1, 0);
}

/** Suspend telemetry task so OTA has as much free heap as possible. */
static void _ota_suspend(void)
{
    if (telemetry_task_handle) vTaskSuspend(telemetry_task_handle);
    ESP_LOGI("OTA", "Telemetry task suspended for OTA");
}

/** Resume telemetry task after a failed OTA (success path calls esp_restart). */
static void _ota_resume(void)
{
    if (telemetry_task_handle) vTaskResume(telemetry_task_handle);
    ESP_LOGI("OTA", "Telemetry task resumed after OTA failure");
}

/* ── Main OTA task ───────────────────────────────────────────────────────── */
/*
 * xTaskCreate(ota_update_task, "ota", 16384, ota_url_g, 5, NULL)
 *                                      ↑ 16 KB — mbedTLS handshake needs
 *                                        at least 12 KB of call-chain stack
 *
 * Flow:
 *  1. Suspend sensor tasks (free heap for TLS context: ~50 KB per session)
 *  2. Publish progress = 0
 *  3. esp_https_ota_begin  → TLS handshake + HTTP GET via Espressif's
 *                             tested HTTPS-OTA component
 *  4. esp_https_ota_perform (loop) → stream-write to OTA partition
 *  5. Publish progress every 5 %
 *  6. esp_https_ota_finish → set boot partition
 *  7. Publish result:success + reboot
 *     (on any failure: publish result:failed + resume tasks)
 */
void ota_update_task(void *pvParameter)
{
    const char *url = (const char *)pvParameter;
    ESP_LOGI("OTA", "Starting OTA from: %s", url);

    _ota_suspend();
    _ota_pub("progress", "{\"status\":\"starting\",\"progress\":0}");

    /* ── Configure HTTPS connection ───────────────────────────────────────
     * crt_bundle_attach: use the full built-in CA certificate bundle
     *   (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y ensures Let's
     *   Encrypt / DigiCert / ISRG Root X1 are all present)
     * timeout_ms: 30 s — Render.com cold-starts can be slow
     * keep_alive_enable: avoids TCP idle-timeout mid-download              */
    esp_http_client_config_t hcfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 30000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &hcfg,
    };

    /* ── Begin OTA (TLS handshake + HTTP GET happens here) ────────────── */
    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_https_ota_begin: %s", esp_err_to_name(err));
        _ota_pub("result", "{\"status\":\"failed\",\"reason\":\"ota_begin\"}");
        _ota_resume();
        vTaskDelete(NULL);
        return;
    }

    int image_size = esp_https_ota_get_image_size(ota_handle);
    ESP_LOGI("OTA", "Firmware size: %d bytes", image_size);

    /* ── Stream download → OTA flash partition ────────────────────────── */
    int last_pct = -1;

    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;

        /* Report progress every 5 % (avoid flooding MQTT) */
        if (image_size > 0) {
            int received = esp_https_ota_get_image_len_read(ota_handle);
            int pct = (received * 100) / image_size;
            if (pct != last_pct && pct % 5 == 0) {
                char msg[72];
                snprintf(msg, sizeof(msg),
                         "{\"status\":\"downloading\",\"progress\":%d}", pct);
                _ota_pub("progress", msg);
                last_pct = pct;
                ESP_LOGI("OTA", "Progress: %d%%", pct);
            }
        }
    }

    /* err is now either ESP_OK (all data received) or an error code */
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_https_ota_perform: %s", esp_err_to_name(err));
        esp_https_ota_abort(ota_handle);
        _ota_pub("result", "{\"status\":\"failed\",\"reason\":\"download_failed\"}");
        _ota_resume();
        vTaskDelete(NULL);
        return;
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ESP_LOGE("OTA", "Incomplete data received");
        esp_https_ota_abort(ota_handle);
        _ota_pub("result", "{\"status\":\"failed\",\"reason\":\"incomplete\"}");
        _ota_resume();
        vTaskDelete(NULL);
        return;
    }

    /* ── Finalise: verify image + set boot partition ──────────────────── */
    err = esp_https_ota_finish(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_https_ota_finish: %s", esp_err_to_name(err));
        _ota_pub("result", "{\"status\":\"failed\",\"reason\":\"ota_finish\"}");
        _ota_resume();
        vTaskDelete(NULL);
        return;
    }

    /* ── Success: notify app then reboot ──────────────────────────────── */
    _ota_pub("progress", "{\"status\":\"rebooting\",\"progress\":100}");
    _ota_pub("result",   "{\"status\":\"success\",\"progress\":100}");
    ESP_LOGI("OTA", "OTA successful — restarting in 1 s");
    vTaskDelay(pdMS_TO_TICKS(1000));   /* let MQTT flush the publish queue */
    esp_restart();
}

#endif /* OTA_MANAGER_H */
