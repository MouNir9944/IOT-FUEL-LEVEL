// modem_status_monitor.c
#include "modem_status_monitor.h"
#include "modem_event_handler.h"
#include "modem_mqtt_handler.h"
#include "SIM7600.h"
#include <string.h>
#include <stdio.h>
//#include "SIM7600.h"
static const char *TAG_MODEM_STATUS = "MODEM STATUS";

// Status monitoring task handle
TaskHandle_t status_monitor_task_handle = NULL;

// Current modem status structure
static modem_status_t current_status = {
    .sim_ready = false,
    .network_registered = false,
    .data_connected = false,
    .mqtt_connected = false,
    .signal_dbm = 0,
    .ip_address = {0},
    .last_check_time = 0
};

// Recovery counters
static struct {
    uint8_t mqtt_failures;
    uint8_t data_failures;
    uint8_t network_failures;
    uint8_t sim_failures;
    TickType_t last_recovery_time;
    TickType_t last_mqtt_check_time;
} recovery_stats = {0};


 esp_err_t check_mqtt_connection(void);
static esp_err_t check_data_connection(char *ip_addr, size_t ip_len);
static esp_err_t check_network_registration(void);
static int check_signal_quality(void);
static esp_err_t check_sim_status(void);
static void handle_recovery(modem_status_check_t failed_check);

/**
 * @brief Initialize the modem status monitor
 */
esp_err_t modem_status_monitor_init(void) {
    ESP_LOGI(TAG_MODEM_STATUS, "Initializing modem status monitor");
    
    // Reset recovery stats
    memset(&recovery_stats, 0, sizeof(recovery_stats));
    recovery_stats.last_recovery_time = xTaskGetTickCount();
    recovery_stats.last_mqtt_check_time = xTaskGetTickCount();
    

    
    ESP_LOGI(TAG_MODEM_STATUS, "Modem status monitor initialized (suspended)");
    return ESP_OK;
}

/**
 * @brief Start the status monitor (called after MQTT connected)
 */
esp_err_t modem_status_monitor_start(void) {
    if (status_monitor_task_handle==NULL) {
        ESP_LOGI(TAG_MODEM_STATUS, "Starting modem status monitor");

        return ESP_OK;
    }else{
          vTaskResume(status_monitor_task_handle);
    }
    return ESP_FAIL;
}

/**
 * @brief Stop the status monitor
 */
esp_err_t modem_status_monitor_stop(void) {
    if (status_monitor_task_handle) {
        ESP_LOGI(TAG_MODEM_STATUS, "Stopping modem status monitor");
        vTaskSuspend(status_monitor_task_handle);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Get current modem status
 */
modem_status_t modem_get_status(void) {
    return current_status;
}

/**
 * @brief Main status monitoring task - REVERSE ORDER CHECKING
 */
void status_monitor_task(void *pvParameters) {
    //TickType_t last_check_time = xTaskGetTickCount();
    
    // Start suspended - will be resumed after MQTT connects
  
    
   // while (1) {
        // Wait for next check interval (30 seconds for normal operation)
       // vTaskDelayUntil(&last_check_time, pdMS_TO_TICKS(STATUS_CHECK_INTERVAL_MS));
        
       // ESP_LOGD(TAG_MODEM_STATUS, "Starting modem status check (reverse order)");
       // current_status.last_check_time = xTaskGetTickCount();

        // STEP 1: Check MQTT connection first (most critical)
        while (check_mqtt_connection() != ESP_OK) {
            ESP_LOGW(TAG_MODEM_STATUS, "MQTT not connected - checking lower layers");
            current_status.mqtt_connected = false;
            recovery_stats.mqtt_failures++;

            if (recovery_stats.mqtt_failures >= 5) {
                ESP_LOGE(TAG_MODEM_STATUS, "MQTT failure threshold reached, initiating recovery");
                mqtt_main_task_stop();
                // If MQTT fails, check data connection
                char ip_addr[16] = {0};
                if (check_data_connection(ip_addr, sizeof(ip_addr)) != ESP_OK) {
                    ESP_LOGW(TAG_MODEM_STATUS, "Data not connected - checking network");
                    current_status.data_connected = false;
                    strcpy(current_status.ip_address, "0.0.0.0");
                    recovery_stats.data_failures++;
                    
                    // If data fails, check network registration
                    if (check_network_registration() != ESP_OK) {
                        ESP_LOGW(TAG_MODEM_STATUS, "Network not registered - checking SIM");
                        current_status.network_registered = false;
                        recovery_stats.network_failures++;
                        
                        // If network fails, check SIM
                        if (check_sim_status() != ESP_OK) {
                            ESP_LOGW(TAG_MODEM_STATUS, "SIM not ready - bottom layer failure");
                            current_status.sim_ready = false;
                            recovery_stats.sim_failures++;
                            handle_recovery(MODEM_STATUS_SIM);
                        } else {
                            current_status.sim_ready = true;
                            recovery_stats.sim_failures = 0;
                            handle_recovery(MODEM_STATUS_NETWORK);
                            handle_recovery(MODEM_STATUS_DATA); 
                        }
                    } else {
                        current_status.network_registered = true;
                        recovery_stats.network_failures = 0;
                        
                        // Network OK but data failed - try data recovery
                        int signal = check_signal_quality();
                        current_status.signal_dbm = signal;
                        handle_recovery(MODEM_STATUS_DATA);
                    }
                } else {
                    // Data OK but MQTT failed - try MQTT recovery
                    current_status.data_connected = true;
                    strncpy(current_status.ip_address, ip_addr, sizeof(current_status.ip_address) - 1);
                    recovery_stats.data_failures = 0;
                    
                    // Get signal quality
                    int signal = check_signal_quality();
                    current_status.signal_dbm = signal;
                    
                    handle_recovery(MODEM_STATUS_MQTT);
                }
            }

        }
            current_status.mqtt_connected = true;
            recovery_stats.mqtt_failures = 0;
            // Log current status
            mqtt_main_task_start();
            ESP_LOGI(TAG_MODEM_STATUS, "All systems normal, Status: MQTT=%s",current_status.mqtt_connected ? "OK" : "FAIL");
            vTaskDelete(NULL);
}

/**
 * @brief Check MQTT connection using AT+SMSTATE? (FIRST CHECK)
 */
esp_err_t check_mqtt_connection(void) {
    // Throttle MQTT checks to avoid flooding (check every 30 seconds max)
   // TickType_t now = xTaskGetTickCount();
    //if ((now - recovery_stats.last_mqtt_check_time) < pdMS_TO_TICKS(30000)) {
        // Return cached status if checking too frequently
    //    return current_status.mqtt_connected ? ESP_OK : ESP_FAIL;
   // }
    
    flush_at_response_queue();
    UART_sendd("AT+SMSTATE?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    char* response = UART_receive(1000);
   // recovery_stats.last_mqtt_check_time = now;
    
    if (!response) {
        ESP_LOGW(TAG_MODEM_STATUS, "No response to SMSTATE query");
        return ESP_FAIL;
    }
    
    esp_err_t ret = ESP_FAIL;
    
    // Look for +SMSTATE: 1 (connected)
    if (strstr(response, "+SMSTATE: 1") || strstr(response, "+SMSTATE:1")) {
        ESP_LOGD(TAG_MODEM_STATUS, "MQTT connected");
        ret = ESP_OK;
    } else if (strstr(response, "+SMSTATE: 0")) {
        ESP_LOGD(TAG_MODEM_STATUS, "MQTT disconnected");
    }
    
    freeReceivedMessage(response);
    return ret;
}

/**
 * @brief Check data connection and get IP address using AT+CNACT? (SECOND CHECK)
 */
static esp_err_t check_data_connection(char *ip_addr, size_t ip_len) {
    flush_at_response_queue();
    UART_sendd("AT+CNACT?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    char* response = UART_receive(1000);
    if (!response) {
        ESP_LOGW(TAG_MODEM_STATUS, "No response to CNACT query");
        return ESP_FAIL;
    }
    
    esp_err_t ret = ESP_FAIL;
    
    // Parse +CNACT: 1,"192.168.1.100"
    char *cnact = strstr(response, "+CNACT:");
    if (cnact) {
        int active;
        char ip[16] = {0};
        if (sscanf(cnact, "+CNACT: %d,\"%15[^\"]\"", &active, ip) == 2) {
            if (active == 1 && strcmp(ip, "0.0.0.0") != 0 && strlen(ip) > 0) {
                strncpy(ip_addr, ip, ip_len);
                ESP_LOGD(TAG_MODEM_STATUS, "Data connected, IP: %s", ip_addr);
                ret = ESP_OK;
            } else {
                ESP_LOGD(TAG_MODEM_STATUS, "Data not connected (IP: %s)", ip);
            }
        }
    }
    
    freeReceivedMessage(response);
    return ret;
}

/**
 * @brief Check network registration using AT+CREG? (THIRD CHECK)
 */
static esp_err_t check_network_registration(void) {
    flush_at_response_queue();
    UART_sendd("AT+CREG?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    char* response = UART_receive(1000);
    if (!response) {
        ESP_LOGW(TAG_MODEM_STATUS, "No response to CREG query");
        return ESP_FAIL;
    }
    
    esp_err_t ret = ESP_FAIL;
    // Look for +CREG: 0,1 or +CREG: 0,5 (registered, home or roaming)
    if (strstr(response, "+CREG: 0,1") || strstr(response, "+CREG: 0,5")) {
        ESP_LOGD(TAG_MODEM_STATUS, "Network registered");
        ret = ESP_OK;
    } else if (strstr(response, "+CREG: 0,2")) {
        ESP_LOGD(TAG_MODEM_STATUS, "Network searching...");
    } else if (strstr(response, "+CREG: 0,3")) {
        ESP_LOGW(TAG_MODEM_STATUS, "Network registration denied");
    } else if (strstr(response, "+CREG: 0,0")) {
        ESP_LOGD(TAG_MODEM_STATUS, "Not registered");
    }
    
    freeReceivedMessage(response);
    return ret;
}

/**
 * @brief Check signal quality (ALWAYS CHECKED)
 */
static int check_signal_quality(void) {
    flush_at_response_queue();
    UART_sendd("AT+CSQ\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    char* response = UART_receive(1000);
    if (!response) {
        ESP_LOGW(TAG_MODEM_STATUS, "No response to CSQ query");
        return -1;
    }
    
    int csq = -1;
    int dbm = -1;
    
    // Parse the CSQ value from the response
    if (sscanf(response, "+CSQ: %d", &csq) == 1) {
        if (csq >= 0 && csq <= 31) {  // Valid CSQ range
            dbm = csq * 2 - 113;  // Convert to dBm
        } else if (csq == 99) {
            dbm = -1;
        } else {
            dbm = -1;
        }
    }
    
    freeReceivedMessage(response);
    return dbm;
}

/**
 * @brief Check SIM status using AT+CPIN? (FOURTH CHECK - BOTTOM LAYER)
 */
static esp_err_t check_sim_status(void) {
    flush_at_response_queue();
    UART_sendd("AT+CPIN?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    char* response = UART_receive(1000);
    if (!response) {
        ESP_LOGW(TAG_MODEM_STATUS, "No response to CPIN query");
        return ESP_FAIL;
    }
    
    esp_err_t ret = ESP_FAIL;
    if (strstr(response, "+CPIN: READY") || strstr(response, "READY")) {
        ESP_LOGD(TAG_MODEM_STATUS, "SIM ready");
        ret = ESP_OK;
    } else if (strstr(response, "+CPIN: SIM PIN")) {
        ESP_LOGW(TAG_MODEM_STATUS, "SIM waiting for PIN");
    } else if (strstr(response, "+CPIN: SIM PUK")) {
        ESP_LOGE(TAG_MODEM_STATUS, "SIM PUK required");
    } else if (strstr(response, "ERROR")) {
        ESP_LOGW(TAG_MODEM_STATUS, "CPIN query error");
    }
    
    freeReceivedMessage(response);
    return ret;
}

/**
 * @brief Handle recovery based on which check failed
 */
static void handle_recovery(modem_status_check_t failed_check) {
    TickType_t now = xTaskGetTickCount();
    
    // Prevent too frequent recovery attempts (at least 60 seconds between full recoveries)
    if ((now - recovery_stats.last_recovery_time) < pdMS_TO_TICKS(60000)) {
        // Fix: Use %u for unsigned int or %lu for unsigned long
        ESP_LOGI(TAG_MODEM_STATUS, "Full recovery throttled, waiting %lu ms",
                 (unsigned long)(60000 - pdTICKS_TO_MS(now - recovery_stats.last_recovery_time)));
        return;
    }
    
    // Check failure thresholds (lower thresholds for faster recovery)
    switch (failed_check) {
        case MODEM_STATUS_MQTT:
            if (recovery_stats.mqtt_failures >= 2) {
                ESP_LOGW(TAG_MODEM_STATUS, "MQTT connection lost, reconnecting...");
                mqtt_handler_start();
                //mqtt_is_connected();
                recovery_stats.mqtt_failures = 0;
                recovery_stats.last_recovery_time = now;
            }
            break;
            
        case MODEM_STATUS_DATA:
            if (recovery_stats.data_failures >= 2) {
                ESP_LOGW(TAG_MODEM_STATUS, "Data connection lost, reconnecting...");
                modem_connect_data() ;
                recovery_stats.data_failures = 0;
                recovery_stats.last_recovery_time = now;
            }
            break;
            
        case MODEM_STATUS_NETWORK:
            if (recovery_stats.network_failures >= 2) {
                ESP_LOGW(TAG_MODEM_STATUS, "Network registration failures, forcing re-registration");
                modem_register_network();
                recovery_stats.network_failures = 0;
                recovery_stats.last_recovery_time = now;
            }
            break;
            
        case MODEM_STATUS_SIM:
            if (recovery_stats.sim_failures >= 2) {
                ESP_LOGE(TAG_MODEM_STATUS, "SIM failures threshold reached, restarting modem");
                SIM7600_restart_modem_via_at();
                 vTaskDelay(pdMS_TO_TICKS(1000));
                 modem_initialize();
                 recovery_stats.network_failures = 0;
                 recovery_stats.data_failures = 0;
                 recovery_stats.mqtt_failures = 0;
                 recovery_stats.sim_failures = 0;
                esp_event_post(MODEM_EVENT, MODEM_EVENT_ERROR, NULL, 0, portMAX_DELAY);
                recovery_stats.sim_failures = 0;
                recovery_stats.last_recovery_time = now;
            }
            break;
            
        default:
            break;
    }
}