// modem_mqtt_handler.h
#ifndef MODEM_MQTT_HANDLER_H
#define MODEM_MQTT_HANDLER_H

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Event base for MQTT events
ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

// MQTT event IDs
typedef enum {
    MQTT_EVENT_CONNECTED = 0,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_MESSAGE_RECEIVED,
    MQTT_EVENT_ERROR,
} mqtt_event_id_t;

// Message received event data
typedef struct {
    char topic[128];
    char payload[512];
} mqtt_message_event_t;

// Public API
esp_err_t mqtt_handler_init(void);
esp_err_t mqtt_handler_start(void);
esp_err_t mqtt_handler_stop(void);

esp_err_t mqtt_main_task_start(void);
esp_err_t mqtt_main_task_stop(void);
void mqtt_main_task(void *pvParameters) ;

esp_err_t mqtt_set_broker(const char *broker, const char *username, const char *password);
esp_err_t mqtt_set_client_id(const char *client_id);
esp_err_t mqtt_set_will(const char *topic, const char *message);
esp_err_t mqtt_publish_logs(const char *topic, const char *err,const char *code,const char *ermessage, uint8_t qos) ;
esp_err_t mqtt_publish(const char *topic, const char *payload, uint8_t qos);
esp_err_t mqtt_subscribe(const char *topic, uint8_t qos);
esp_err_t mqtt_unsubscribe(const char *topic);

bool mqtt_is_connected(void);
esp_err_t mqtt_wait_for_connection(TickType_t timeout);
esp_err_t start_ota_update(const char *url);
esp_err_t start_update_config(const char *config_json);
#ifdef __cplusplus
}
#endif

#endif /* MODEM_MQTT_HANDLER_H */