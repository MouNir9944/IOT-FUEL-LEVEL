// modem_event_handler.h
#ifndef MODEM_EVENT_HANDLER_H
#define MODEM_EVENT_HANDLER_H

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event base declaration
ESP_EVENT_DECLARE_BASE(MODEM_EVENT);

// Modem state enum for waiting functions
typedef enum {
    MODEM_STATE_POWERED_ON,
    MODEM_STATE_POWERED_RESET,  
    MODEM_STATE_INIT,
    MODEM_STATE_REGISTERED,
    MODEM_STATE_DATA_CONNECTED,
    MODEM_STATE_MQTT_CONNECTED,
} modem_state_t;

// Modem event IDs
typedef enum {
    MODEM_EVENT_POWERED_ON = 0,
    MODEM_EVENT_READY,
    MODEM_EVENT_REGISTERED,
    MODEM_EVENT_DATA_CONNECTED,
    MODEM_EVENT_TIME_SYNCED,
    MODEM_EVENT_GPS_STARTED,
    MODEM_EVENT_DATA_DISCONNECTED,
    MODEM_EVENT_ERROR,
    MODEM_EVENT_SIGNAL_UPDATE,
} modem_event_id_t;

// MQTT event IDs
typedef enum {
    MODEM_MQTT_EVENT_CONNECTED = 100,
    MODEM_MQTT_EVENT_DISCONNECTED,
    MODEM_MQTT_EVENT_SUBSCRIBED,
    MODEM_MQTT_EVENT_PUBLISHED,
    MODEM_MQTT_EVENT_DATA,
    MODEM_MQTT_EVENT_ERROR,
} modem_mqtt_event_id_t;

// Event data structures
typedef struct {
    int signal_dbm;
    int network_mode;   /* AT+CNSMOD? <stat>: 0=no service, 8=LTE, etc. (-1=unknown) */
} modem_signal_event_t;

typedef struct {
    char topic[128];
    char payload[512];
} modem_mqtt_data_event_t;

// Public API functions
esp_err_t modem_restart(void);
esp_err_t modem_power_on(void);
esp_err_t modem_initialize(void);
esp_err_t modem_register_network(void);
esp_err_t modem_connect_data(void);
esp_err_t modem_start_gps(void);
esp_err_t modem_get_gps(void);
esp_err_t modem_sync_time(void);
esp_err_t modem_wait_for_state(modem_state_t state, TickType_t timeout);
esp_err_t modem_init(void);
esp_err_t modem_get_signal(void);



#ifdef __cplusplus
}
#endif

#endif /* MODEM_EVENT_HANDLER_H */