// modem_status_monitor.h
#ifndef MODEM_STATUS_MONITOR_H
#define MODEM_STATUS_MONITOR_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Status check interval (30 seconds for normal operation)
#define STATUS_CHECK_INTERVAL_MS 30000

// Modem status structure
typedef struct {
    bool sim_ready;
    bool network_registered;
    bool data_connected;
    bool mqtt_connected;
    int signal_dbm;  // in dBm
    char ip_address[16];
    TickType_t last_check_time;
} modem_status_t;

// Types of status checks for recovery
typedef enum {
    MODEM_STATUS_MQTT,
    MODEM_STATUS_DATA,
    MODEM_STATUS_NETWORK,
    MODEM_STATUS_SIM
} modem_status_check_t;

 esp_err_t check_mqtt_connection(void);

// Initialize the status monitor (creates suspended task)
esp_err_t modem_status_monitor_init(void);

// Start monitoring (call after MQTT connected)
esp_err_t modem_status_monitor_start(void);

// Stop monitoring
esp_err_t modem_status_monitor_stop(void);

// Get current modem status
modem_status_t modem_get_status(void);
void status_monitor_task(void *pvParameters);
#endif // MODEM_STATUS_MONITOR_H