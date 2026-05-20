/*
 * Copyright (c) 2024 <[SFM Technologies]>
 */
#ifndef SENSOR_FUEL_MANAGER_H
#define SENSOR_FUEL_MANAGER_H
#include "cJSON.h"
#include "esp_err.h"
#include "esp_event.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include "esp_system.h"
 #include "driver/gpio.h"
#include "driver/uart.h"
 #include "esp_log.h"

#include <time.h>

#define UART_NUM_2 UART_NUM_2
#define TXD_PIN_2 GPIO_NUM_21
#define RXD_PIN_2 GPIO_NUM_22
#define DERE GPIO_NUM_32
#define UART_BUFFER_SIZE_2 1024
#define BUF_SIZE_2 10240
#define RESPONSE_BUFFER_SIZE_2 1024
void gpio_init_control_rs485( gpio_num_t gpio_rs485);
void bat_adc_init(void);
int read_battery_mv(void); 
void create_and_send_json_request_soji_sensor_fuel_level( cJSON**payload_str);
//esp_err_t modbus_rs485_master_init(void);
esp_err_t UART_init_SOJI(void);
void send_data_with_checksum(uint8_t *data, int length);
cJSON *soji_sensor_fuel_level(uint8_t send_data[],int len);
uint8_t crc8(uint8_t data, uint8_t crc) ;
uint8_t calculate_crc8_checksum(uint8_t *data, int length);
cJSON *parse_response(uint8_t *data, size_t length);
esp_err_t save_failed_payload_to_sd(const char* payload, int error_code);

#endif /* SENSOR_FUEL_MANAGER_H */