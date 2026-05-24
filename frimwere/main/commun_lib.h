/* commun_lib.h — shared includes and global declarations for IOT-FUEL-LEVEL
 *
 * This file is included by multiple headers (nvs_manager.h, spiffs_manager.h,
 * wifi_manager.h, mqtt_manager.h).  All globals here are declared extern so
 * the header can be safely included from several translation units without
 * causing "multiple definition" linker errors.
 *
 * Actual definitions live in main.c:
 *   char g_conn_mode[8]    = "modem";
 *   char g_apn[32]         = "internet";
 *   char g_operator_mnc[8] = "";
 */

#ifndef COMMUN_LIB_H
#define COMMUN_LIB_H

/* ── Standard C ────────────────────────────────────────────────────────────── */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* ── FreeRTOS ──────────────────────────────────────────────────────────────── */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

/* ── ESP-IDF core ──────────────────────────────────────────────────────────── */
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"

/* ── cJSON ─────────────────────────────────────────────────────────────────── */
#include "cJSON.h"

/* ── LED status indicators ─────────────────────────────────────────────────── */
#include "led_manager.h"

/* ── Connectivity mode ─────────────────────────────────────────────────────── *
 * "wifi"  → ESP MQTT client over WiFi                                          *
 * "modem" → SIM7600 AT MQTT over cellular (default for this project)           *
 * Defined once in main.c; declared extern here so all modules share it.        */
extern char g_conn_mode[8];

/* APN string used by the modem driver */
extern char g_apn[32];

/* Mobile operator MNC code (empty = auto-select)
 *   60501 = Tunisie Telecom
 *   60502 = Ooredoo
 *   60503 = Orange                                                              */
extern char g_operator_mnc[8];

#endif /* COMMUN_LIB_H */
