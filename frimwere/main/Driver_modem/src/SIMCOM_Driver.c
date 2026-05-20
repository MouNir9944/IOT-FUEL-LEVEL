/*
 * UART/AT Command Handling:
 * - Only uart_dispatcher_task reads from UART.
 * - All AT command responses are dynamically allocated and must be freed by the caller using freeReceivedMessage.
 * - All unsolicited messages (e.g., MQTT) are handled in the dispatcher and do not go to the AT response queue.
 * - Always check for NULL before using or printing UART_receive results.
 * 
 * UART Error Handling Improvements:
 * - Added adaptive polling delays to reduce UART driver errors
 * - Implemented UART health checks and error recovery
 * - Added timeout handling to prevent continuous error loops
 * - Improved error handling in UART_read_line function
 */
#include "SIMCOM_Driver.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "modem_mqtt_handler.h"
#include "cJSON.h"
#include "SIM7600.h"
#include "device_config.h"


static const char *TAG_SIMCOM_DRIVER = "SIMCOM DRIVER";

#define UART_LINE_MAX_LEN 256
QueueHandle_t at_response_queue = NULL;

esp_err_t UART_init_SIMcom() {
    uart_config_t uartconfig= {
        .baud_rate =  115200,
        .data_bits = UART_DATA_8_BITS,
        .parity =  UART_PARITY_DISABLE,
        .stop_bits= UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk =UART_SCLK_APB,
    };
    esp_err_t ret = uart_param_config(UART_NUM, &uartconfig);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SIMCOM_DRIVER, "UART param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SIMCOM_DRIVER, "UART set pin failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_driver_install(UART_NUM, RESPONSE_BUFFER_SIZE, BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SIMCOM_DRIVER, "UART driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Flush any existing data in UART buffers
    uart_flush(UART_NUM);
    
    ESP_LOGI(TAG_SIMCOM_DRIVER, "UART initialized successfully");
    return ESP_OK;
}
//void gpio_init_control_SIMcom( gpio_num_t gpio_modem_key, gpio_num_t gpio_modem_DTR){
void gpio_init_control_SIMcom( gpio_num_t gpio_modem_key){
    // Configure modem GPIOs as output
    gpio_config_t gpio_modem_config = {
        .pin_bit_mask = (1ULL << gpio_modem_key),
         //.pin_bit_mask = (1ULL << gpio_modem_key) | (1ULL << gpio_modem_DTR),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    esp_err_t err = gpio_config(&gpio_modem_config);
    if (err != ESP_OK) {
       // ESP_LOGD(TAG_SIMCOM_DRIVER, "GPIO configuration failed for pins %d and %d: %s", gpio_modem_key, gpio_modem_DTR, esp_err_to_name(err));
                ESP_LOGD(TAG_SIMCOM_DRIVER, "GPIO configuration failed for pins %d : %s", gpio_modem_key, esp_err_to_name(err));

    }
        gpio_set_level(gpio_modem_key, 0);
       // gpio_set_level(gpio_modem_DTR, 0);
}
void modemPowerOn() {
    gpio_set_level(powerkey, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(powerkey, 0); 
    vTaskDelay(2500 / portTICK_PERIOD_MS); 
    gpio_set_level(powerkey, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(powerkey, 0); 
    vTaskDelay(2500/ portTICK_PERIOD_MS); 
    gpio_set_level(powerkey, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(powerkey, 0); 
    ESP_LOGI(TAG_SIMCOM_DRIVER, "Modem Power ON ");
}
void modemPowerOff() {
    gpio_set_level(powerkey, 1);
    vTaskDelay(2500 / portTICK_PERIOD_MS);
    gpio_set_level(powerkey, 0); 
    ESP_LOGI(TAG_SIMCOM_DRIVER, "Modem Power off ");
}
esp_err_t  modemRestart() {
 modemPowerOff();
 vTaskDelay(10000 / portTICK_PERIOD_MS);
 modemPowerOn();
 ESP_LOGI(TAG_SIMCOM_DRIVER, "Modem Power Restart ");
 return ESP_OK;
}
void UART_sendd(const char *command) {
    /* Debug: log every AT command sent to the modem (strip trailing \r\n for readability) */
    size_t cmd_len = strlen(command);
    while (cmd_len > 0 && (command[cmd_len - 1] == '\r' || command[cmd_len - 1] == '\n')) {
        cmd_len--;
    }
    ESP_LOGD(TAG_SIMCOM_DRIVER, "AT >> %.*s", (int)cmd_len, command);

    uart_write_bytes(UART_NUM, command, strlen(command));
}
char* UART_receive(int rx_timeout) {
    return wait_for_at_response(rx_timeout);
}
void freeReceivedMessage(char* received_message) {
    if (received_message != NULL) {
        free(received_message);
    }
}

// Function to handle UART errors and attempt recovery
static void uart_error_recovery(void) {
    ESP_LOGW(TAG_SIMCOM_DRIVER, "UART error detected, attempting recovery...");
    
    // Small delay to let UART stabilize
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG_SIMCOM_DRIVER, "UART recovery completed");
}

// Function to check UART health
static bool uart_health_check(void) {
    // Try to read a single byte with a very short timeout
    uint8_t test_byte;
    int len = uart_read_bytes(UART_NUM, &test_byte, 1, 1 / portTICK_PERIOD_MS);
    
    // If we get an error (len < 0), UART might be in bad state
    if (len < 0) {
        ESP_LOGW(TAG_SIMCOM_DRIVER, "UART health check failed");
        return false;
    }
    
    return true;
}
//Reads a line from UART into buf, up to max_len. Returns number of chars read.
int UART_read_line(char *buf, int max_len) {
    int idx = 0;
    char c = 0;
    int timeout_count = 0;
    const int max_timeouts = 3; // Allow a few timeouts before giving up
    
    while (idx < max_len - 1) {
        int len = uart_read_bytes(UART_NUM, (uint8_t*)&c, 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            timeout_count = 0; // Reset timeout counter on successful read
            if (c == '\n' || c == '\r') {
                if (idx == 0) continue; // skip leading newlines
                break;
            }
            buf[idx++] = c;
        } else if (len == 0) {
            // Timeout, no data
            timeout_count++;
            if (idx == 0 && timeout_count >= max_timeouts) {
                return 0; // No data read after multiple timeouts
            }
            if (idx > 0) {
                break; // We have some data, return it
            }
        } else {
            // Error occurred
            if (idx == 0) {
                // If no data read and error occurred, check UART health
                if (!uart_health_check()) {
                    uart_error_recovery();
                }
                continue;
            }
            break; // Return what we have so far
        }
    }
    buf[idx] = '\0';
    return idx;
}

void handle_incoming_mqtt_message(const char* topic, const char* payload) {
    ESP_LOGI(TAG_SIMCOM_DRIVER, "Received MQTT message on topic '%s': %s", topic, payload);

                // Parse JSON payload
                cJSON *root = cJSON_Parse(payload);
                if (root == NULL) {
                    ESP_LOGE(TAG_SIMCOM_DRIVER, "Failed to parse JSON");
                    return;
                }
    /* Build correct topics from real device MAC */
    const char *mac = get_device_mac_str();
    char config_topic[128];
    char ota_topic[128];
    char cmd_topic[128];
    char config_report_topic[128];
    snprintf(config_topic,        sizeof(config_topic),        "device/%s/config",        mac);
    snprintf(ota_topic,           sizeof(ota_topic),           "device/%s/ota",           mac);
    snprintf(cmd_topic,           sizeof(cmd_topic),           "device/%s/cmd",           mac);
    snprintf(config_report_topic, sizeof(config_report_topic), "device/%s/config_report", mac);

                    if (strcmp(topic, ota_topic) == 0) {
                        ESP_LOGI(TAG_SIMCOM_DRIVER, "OTA update command received");
                        cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
                        if (cJSON_IsString(cmd_item) && cmd_item->valuestring != NULL) {
                            ESP_LOGW(TAG_SIMCOM_DRIVER, "update ota command received:");
                            if (strcmp(cmd_item->valuestring, "ota_update") == 0) {
                                cJSON *url_item = cJSON_GetObjectItem(root, "url");
                                const char *ota_url = (cJSON_IsString(url_item) && url_item->valuestring)
                                                      ? url_item->valuestring : NULL;
                                ESP_LOGI(TAG_SIMCOM_DRIVER, "OTA URL: %s", ota_url ? ota_url : "(none)");
                                start_ota_update(ota_url);
                            }
                        }
                    } else if (strcmp(topic, config_topic) == 0) {
                        ESP_LOGW(TAG_SIMCOM_DRIVER, "update config command received:");
                        start_update_config(payload);
                    } else if (strcmp(topic, cmd_topic) == 0) {
                        cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
                        if (cJSON_IsString(cmd_item) && cmd_item->valuestring != NULL) {
                            if (strcmp(cmd_item->valuestring, "report_config") == 0) {
                                ESP_LOGI(TAG_SIMCOM_DRIVER, "report_config command received — publishing current config");
                                char *cfg_json = device_config_to_json_str();
                                if (cfg_json) {
                                    mqtt_publish(config_report_topic, cfg_json, 1);
                                    ESP_LOGI(TAG_SIMCOM_DRIVER, "Config reported to platform on %s", config_report_topic);
                                    free(cfg_json);
                                } else {
                                    ESP_LOGE(TAG_SIMCOM_DRIVER, "report_config: failed to serialize config");
                                }
                            } else {
                                ESP_LOGW(TAG_SIMCOM_DRIVER, "Unknown cmd: %s", cmd_item->valuestring);
                            }
                        }
                    } else {
                        ESP_LOGW(TAG_SIMCOM_DRIVER, "Unknown topic: %s", topic);
                    }
                // Free JSON object
                cJSON_Delete(root);

}

void uart_dispatcher_task(void *pvParameters) {
    char line[UART_LINE_MAX_LEN];
    char topic[128] = {0};
    char payload[512] = {0};
    int in_rx = 0;
    int consecutive_no_data = 0;
    const int max_consecutive_no_data = 20; // Increased threshold
    TickType_t last_activity = xTaskGetTickCount();

    while (1) {
        int bytes_read = UART_read_line(line, sizeof(line));
        if (bytes_read > 0) {
            consecutive_no_data = 0; // Reset counter when we get data
            last_activity = xTaskGetTickCount(); // Update activity timestamp
           // ESP_LOGD(TAG_SIMCOM_DRIVER,"[DISPATCHER] Received: %s\n", line);
            if (strstr(line, "+CMQTTRXSTART")) {
                in_rx = 1;
                topic[0] = '\0';
                payload[0] = '\0';
            } else if (in_rx && strstr(line, "+CMQTTRXTOPIC")) {
                UART_read_line(topic, sizeof(topic));
            } else if (in_rx && strstr(line, "+CMQTTRXPAYLOAD")) {
                UART_read_line(payload, sizeof(payload));
            } else if (in_rx && strstr(line, "+CMQTTRXEND")) {
                handle_incoming_mqtt_message(topic, payload);
                in_rx = 0;
            } else {
                // Not an MQTT message, put in AT response queue
                if (at_response_queue) {
                    // Copy line to heap for queue (so caller can free it)
                    char *resp = strdup(line);
                    xQueueSend(at_response_queue, &resp, portMAX_DELAY);
                }
            }
            // Short delay after processing data
            vTaskDelay(pdMS_TO_TICKS(5));
        } else {
            consecutive_no_data++;
            
            // Calculate time since last activity
            TickType_t current_time = xTaskGetTickCount();
            TickType_t time_since_activity = current_time - last_activity;
            
            // Adaptive delay based on consecutive no-data cycles and time since last activity
            if (consecutive_no_data > max_consecutive_no_data || time_since_activity > pdMS_TO_TICKS(5000)) {
                vTaskDelay(pdMS_TO_TICKS(200)); // Much longer delay when no data for a while
            } else if (consecutive_no_data > 10) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Longer delay when no data
            } else {
                vTaskDelay(pdMS_TO_TICKS(30)); // Normal delay
            }
        }
    }
}
// Flushes the AT response queue to remove old/unsolicited responses
void flush_at_response_queue(void) {
    char *resp;
    while (xQueueReceive(at_response_queue, &resp, 0) == pdTRUE) {
        free(resp);
    }
}
// Waits for an AT command response, skipping echoes (e.g., "AT")
char* wait_for_at_response(int timeout_ms) {
    char *resp = NULL;
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < (timeout_ms / portTICK_PERIOD_MS)) {
        if (xQueueReceive(at_response_queue, &resp, 10 / portTICK_PERIOD_MS) == pdTRUE) {
            /* Skip modem echo: the modem echoes back the command (e.g. "AT+CPIN?").
             * All AT commands start with "AT", while real responses (OK, ERROR,
             * +CPIN:, +CSQ:, etc.) never do — filter on the "AT" prefix. */
            if (strncmp(resp, "AT", 2) == 0) {
                free(resp);  /* silently drop modem echo */
                continue;
            }
            /* Debug: log every real response received from the modem */
            ESP_LOGD(TAG_SIMCOM_DRIVER, "AT << %s", resp);
            return resp; // return first non-echo line
        }
    }
    ESP_LOGD(TAG_SIMCOM_DRIVER, "AT << (timeout after %d ms)", timeout_ms);
    return NULL;
}