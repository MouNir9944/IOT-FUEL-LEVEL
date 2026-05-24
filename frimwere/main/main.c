// main.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "modem_event_handler.h"
#include "modem_mqtt_handler.h"
#include "Sensor_Fuel_Manager.h"
#include "LOG_Save_SD_Manager.h"
#include "device_config.h"
#define SETUP_BUTTON_GPIO  GPIO_NUM_0

/* ── Global connectivity config — declared extern in commun_lib.h ───────────
 * All modules that include commun_lib.h (directly or transitively via
 * nvs_manager.h / wifi_manager.h / spiffs_manager.h / mqtt_manager.h) share
 * these values.  Defined here exactly once so the linker never sees a
 * "multiple definition" error.                                               */
char g_conn_mode[8]    = "modem";   /* "wifi" | "modem" */
char g_apn[32]         = "internet";
char g_operator_mnc[8] = "";        /* "" = auto; "60501/60502/60503" = fixed */

static const char *TAG_MAIN = "TAG MAIN";
esp_err_t ret;
extern TaskHandle_t mqtt_main_task_handler;

void app_main(void) {
    /* ── NVS ────────────────────────────────────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ── Device config (load defaults then NVS overrides) ───────────────── */
    device_config_load_defaults();
    device_config_load_from_nvs();

    /* ── Event loop ─────────────────────────────────────────────────────── */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    bat_adc_init();
    // 1. Initialize SPIFFS (non-fatal — system runs without it)
    ESP_LOGI(TAG_MAIN, "Initializing SPIFFS...");
    ret = sd_card_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_MAIN, "SPIFFS initialized successfully");
    } else {
        ESP_LOGW(TAG_MAIN, "SPIFFS not available (%s) — continuing without it",
                 esp_err_to_name(ret));
    }
    /* ── Cellular modem + MQTT ──────────────────────────────────────────── */
    UART_init_SOJI();
    ESP_ERROR_CHECK(modem_init());
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (mqtt_main_task_handler == NULL) {
        xTaskCreate(mqtt_main_task,"mqtt_pub_task", 12288,NULL,1, &mqtt_main_task_handler);
    } else {
        vTaskResume(mqtt_main_task_handler); // Suspend MQTT task before modem restart
    }
}