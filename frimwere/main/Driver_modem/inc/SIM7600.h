/*
 * Copyright (c) 2024 <[mounir]>
 */



 #ifndef SIM7600_H
 #define SIM7600_H

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdint.h>
 #include <stddef.h>
 #include <stdbool.h>
 #include "esp_err.h"
 #include "esp_mac.h"
 #include "esp_err.h"
 #include "esp_log.h"
 #include "esp_system.h"
 #include <time.h>
#include "SIMCOM_Driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// GPS coordinates structure
typedef struct {
    double latitude;      // Latitude in decimal degrees
    double longitude;     // Longitude in decimal degrees
    char lat_direction;   // 'N' or 'S'
    char lon_direction;   // 'E' or 'W'
    int date;            // DDMMYY format (e.g., 170326 for March 26, 2017)
    float time;          // HHMMSS.S format (e.g., 163518.0)
    float altitude;      // Altitude in meters
    float speed;         // Speed in knots
    // Add more fields as needed
} gps_coordinates_t;




 void read_and_store_mac(void);
 const char* get_device_mac_str(void);
 esp_err_t modemInit_SIM7600(void);
 esp_err_t Registring_SIM7600(void);
 void SIM7600_restart_modem_via_at(void);
 esp_err_t modemconfigmode_SIM7600(char *PreferredModeSelection,char *PreferredSelection,char *APN);
 esp_err_t SIMcom_connect_SIM7600(char *APN);
/***************** */

void parse_gps_info(const char* response);
void wait_for_gps_fix();



void get_iso8601_timestamp(char* buf, size_t len) ;
void parse_gps_info(const char* response);

void start_gps_session();
void get_gps_info();
void enable_gps_auto_start();
void wait_for_gps_fix();
void test_gps_command_support() ;

typedef void (*SIM7600_mqtt_message_callback_t)(const char* topic, const char* payload);

/*
 * Simplified MQTT API — wrappers around SIM7600_MQTT driver.
 * These manage a single internal client instance.
 */
void      SIM7600_mqtt_set_message_callback(SIM7600_mqtt_message_callback_t callback);
esp_err_t SIM7600_mqtt_connect(const char* client_id);
esp_err_t SIM7600_mqtt_set_will(const char* topic, const char* message, uint8_t qos); /* must be called after connect, before set_broker */
esp_err_t SIM7600_mqtt_set_broker(const char* url);   /* e.g. "tcp://host:port" */
esp_err_t SIM7600_mqtt_disconnect(void);
esp_err_t SIM7600_mqtt_publish(const char* topic, const char* payload);
esp_err_t SIM7600_mqtt_publish_data(const char* topic, const char* payload);
esp_err_t SIM7600_mqtt_publish_log(const char* topic, const char* payload);
esp_err_t SIM7600_mqtt_subscribe(const char* topic);
esp_err_t SIM7600_mqtt_unsubscribe(const char* topic);

void sync_modem_time_with_ntp(const char* ntp_server, int timezone);


int get_signal_quality(void);
int get_network_mode(void);
const char *network_mode_to_str(int stat);
void sync_esp32_time_with_modem();
void enable_modem_time_auto_update();
void enable_modem_time_zone_reporting();
void set_esp32_timezone(int timezone_offset);
char* get_modem_time_response();
void wait_for_valid_modem_time() ;

/*********************** */

 #endif  /*SIM7600_H*/