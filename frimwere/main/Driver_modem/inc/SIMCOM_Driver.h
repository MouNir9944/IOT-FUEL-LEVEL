/*
 * Copyright (c) 2024 <[mounir]>
 */
#include "driver/gpio.h"
#include "driver/uart.h"

#include <string.h>
#include "esp_err.h"
#ifndef SIMCOM_DRIVER_H
#define  SIMCOM_DRIVER_H
 

#define UART_BUFFER_SIZE 2024
 

#define UART_NUM UART_NUM_1
#define TXD_PIN GPIO_NUM_27
#define RXD_PIN GPIO_NUM_26 
#define powerkey GPIO_NUM_4
#define DTR GPIO_NUM_25

 // static const char *TAG_SIMcom = "SIMcom Driver"; // Moved to .c file
 
#define BUF_SIZE 10240
#define RESPONSE_BUFFER_SIZE 1024
 
esp_err_t UART_init_SIMcom() ;
//void gpio_init_control_SIMcom( gpio_num_t gpio_modem_key, gpio_num_t gpio_modem_DTR);
    void gpio_init_control_SIMcom( gpio_num_t gpio_modem_key);
void modemPowerOn() ;
void modemPowerOff() ;
esp_err_t  modemRestart() ;
void UART_sendd(const char *command);
char* UART_receive(int rx_timeout) ;
void freeReceivedMessage(char* received_message);
//Reads a line from UART into buf, up to max_len. Returns number of chars read.
int UART_read_line(char *buf, int max_len) ;
void handle_incoming_mqtt_message(const char* topic, const char* payload);
void uart_dispatcher_task(void *pvParameters);
// Flushes the AT response queue to remove old/unsolicited responses
void flush_at_response_queue(void);
// Waits for an AT command response, skipping echoes (e.g., "AT")
char* wait_for_at_response(int timeout_ms) ;
extern QueueHandle_t at_response_queue;






/* Note: SIM7600_get_device_mac_str() and SIM7600_mqtt_publish() are declared
 * in SIM7600.h — include that header directly if you need them. */




#endif /* SIMCOM_DRIVER_H */