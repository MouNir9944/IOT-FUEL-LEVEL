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
 esp_err_t SIM7600_connect_mqtt(const char* client_id, const char* broker_url, const char* will_topic, const char* will_msg, const char* sub_topic, const char* pub_topic, const char* pub_payload);

esp_err_t SIM7600_mqtt_connect(const char* client_id);
esp_err_t SIM7600_mqtt_set_broker(const char* broker_url);
esp_err_t SIM7600_mqtt_subscribe(const char* topic);
esp_err_t SIM7600_mqtt_unsubscribe(const char* topic);
esp_err_t SIM7600_mqtt_publish(const char* topic, const char* payload);
esp_err_t SIM7600_mqtt_disconnect(void);
void SIM7600_mqtt_set_message_callback(SIM7600_mqtt_message_callback_t callback);
*/
void sync_modem_time_with_ntp(const char* ntp_server, int timezone);



void sync_esp32_time_with_modem();
void enable_modem_time_auto_update();
void enable_modem_time_zone_reporting();
void set_esp32_timezone(int timezone_offset);
char* get_modem_time_response();
void wait_for_valid_modem_time() ;

/*********************** */

 #endif  /*SIM7600_H*/