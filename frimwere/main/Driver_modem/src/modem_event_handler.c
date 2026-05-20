// modem_event_handler.c
#include "modem_event_handler.h"
#include "modem_mqtt_handler.h"
//#include "modem_status_monitor.h"
#include "SIM7600.h"
#include "SIMCOM_Driver.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "nvs_Manager.h"

static const char *TAG_MODEM_EVENT_HANDLER = "MODEM EVENT HANDLER";
static bool SYNC_TIME = false;
static bool SYNC_GPS = false;
bool connected=false;
modem_signal_event_t signal_quality = {0};
// Define event base
ESP_EVENT_DEFINE_BASE(MODEM_EVENT);

// Event group for modem states
static EventGroupHandle_t modem_event_group = NULL;
const int MODEM_POWERED_ON = BIT0;
const int MODEM_POWERED_RESET = BIT1;
const int MODEM_INIT_BIT = BIT2;
const int MODEM_REGISTERED_BIT = BIT3;
const int MODEM_GPRS_ATTACHED_BIT = BIT4;
const int MODEM_DATA_CONNECTED_BIT = BIT5;
const int MODEM_MQTT_CONNECTED_BIT = BIT6;

// Queue for event processing
static QueueHandle_t event_command_queue = NULL;

// Command structure for event-driven operations
typedef enum {
    CMD_POWER_RESET,
    CMD_POWER_ON,
    CMD_INIT_MODEM,
    CMD_REGISTER_NETWORK,
    CMD_CONNECT_DATA,
    CMD_START_GPS,
    CMD_GET_GPS,
    CMD_SYNC_TIME,
    CMD_GET_SIGNAL,
} event_cmd_t;

typedef struct {
    event_cmd_t cmd;
    void *data;
    size_t data_len;
} event_command_t;

// MQTT message callback
static void mqtt_message_handler(const char *topic, const char *payload) {
    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "MQTT message received - Topic: %s, Payload: %s", topic, payload);
    
    // Create event data
    modem_mqtt_data_event_t *event_data = malloc(sizeof(modem_mqtt_data_event_t));
    if (event_data) {
        strncpy(event_data->topic, topic, sizeof(event_data->topic) - 1);
        strncpy(event_data->payload, payload, sizeof(event_data->payload) - 1);
        
        // Post event
        esp_event_post(MODEM_EVENT, MODEM_MQTT_EVENT_DATA, 
                      event_data, sizeof(modem_mqtt_data_event_t), portMAX_DELAY);
        free(event_data);
    }
}

// Command processing task - FIXED VERSION
static void modem_command_task(void *pvParameters) {
    event_command_t cmd;
    
    while (1) {
        if (xQueueReceive(event_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.cmd) {
                case CMD_POWER_ON:
                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Power On Modem");
                    modemPowerOn();
                    //modemRestart() ;
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    xEventGroupSetBits(modem_event_group, CMD_POWER_RESET);
                    modem_initialize();
                    break;
                case CMD_POWER_RESET:
                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Reset Modem");
                    SIM7600_restart_modem_via_at();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    //esp_event_post(MODEM_EVENT, MODEM_EVENT_POWERED_ON, NULL, 0, portMAX_DELAY);
                    xEventGroupSetBits(modem_event_group, MODEM_INIT_BIT);
                    modem_initialize();
                    break;
                case CMD_INIT_MODEM:
                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Initialize Modem");
                    if (modemInit_SIM7600() == ESP_OK) {
                        //esp_event_post(MODEM_EVENT, MODEM_EVENT_READY, NULL, 0, portMAX_DELAY);
                        modem_register_network();
                        xEventGroupSetBits(modem_event_group, MODEM_REGISTERED_BIT);
                                               
                    } else {

                        modem_restart();
                        xEventGroupSetBits(modem_event_group, MODEM_INIT_BIT);
                       // esp_event_post(MODEM_EVENT, MODEM_EVENT_ERROR, NULL, 0, portMAX_DELAY);
                    }
                    break;
                    
                case CMD_REGISTER_NETWORK:
                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Network Registration");
                    if ( Registring_SIM7600() == ESP_OK) {
                        //esp_event_post(MODEM_EVENT, MODEM_EVENT_REGISTERED, NULL, 0, portMAX_DELAY);
                        modem_connect_data();
                        xEventGroupSetBits(modem_event_group, MODEM_GPRS_ATTACHED_BIT);
                    }else {
                        modem_restart();
                        xEventGroupSetBits(modem_event_group, MODEM_REGISTERED_BIT);
                       // esp_event_post(MODEM_EVENT, MODEM_EVENT_ERROR, NULL, 0, portMAX_DELAY);
                    }
                    break;
                    
                case CMD_CONNECT_DATA:
                    if (connected==false){
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Data Connection");
                        char *apn = "internet.tn";
                        // STEP 1: Try alternative GPRS connection method
                        if (modemconfigmode_SIM7600("2", "1", apn) == ESP_OK) {
                            ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Modem mode configured successfully");
                        } else {
                            modem_initialize();
                            
                            ESP_LOGW(TAG_MODEM_EVENT_HANDLER, "Modem mode config returned non-OK, continuing anyway...");
                        }
                        /*// STEP 1: Try alternative GPRS connection method
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Step 1: Trying alternative GPRS connection method...");
                        if (modem_connect_GPRS(apn) == ESP_OK) {
                            ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "GPRS connected via SAPBR method!");

                        } else {
                            ESP_LOGE(TAG_MODEM_EVENT_HANDLER, "All connection methods failed");
                            // Don't post REGISTERED again to avoid loop
                        }


                        // STEP 2: Now try data connection
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Step 2: Connecting data with APN: %s", apn);
                                            
                        while ( modem_connect_data(apn) != ESP_OK) {
                            ESP_LOGE(TAG_MODEM_EVENT_HANDLER, "Data connection failed with AT+CGDCONT method, retrying...");
                            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second before retry
                        }*/
                    } else {
                        ESP_LOGW(TAG_MODEM_EVENT_HANDLER, "Data already connected, skipping...");
                    }
                    connected=true;
                    if (SYNC_TIME==false){
                        modem_sync_time();
                        xEventGroupSetBits(modem_event_group, MODEM_DATA_CONNECTED_BIT);
                    }else if(SYNC_GPS==false){
                        if(load_saved_gps_from_nvs()!=ESP_OK){
                            modem_start_gps();
                        }else{
                            SYNC_GPS=true;
                            modem_connect_data();
                        }
                        xEventGroupSetBits(modem_event_group, MODEM_DATA_CONNECTED_BIT);
                    }else{
                        mqtt_set_broker("broker.hivemq.com", "", "");
                        mqtt_set_client_id("ESP32_SIM7000");
                        mqtt_set_will("esp32/sim7000/status", "offline");
                        ESP_ERROR_CHECK(mqtt_handler_init());
                        // Start the process
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Starting MQTT handler..."); 
                        
                        mqtt_handler_start();

                    }
                    break;
                case CMD_SYNC_TIME:
                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Sync Time");
                    // For Tunisia (UTC+1)
                    int local_timezone_hours = 1;  // Tunisia is UTC+1
                    
                    enable_modem_time_auto_update();
                    enable_modem_time_zone_reporting();
                    
                    while (1) {
                        // Pass the timezone in hours, the function will convert to quarters
                        sync_modem_time_with_ntp("pool.ntp.org", local_timezone_hours);
                        
                        char* response = get_modem_time_response();
                        if (response) {
                            ESP_LOGD(TAG_MODEM_EVENT_HANDLER, "modem time: %s", response);
                            char* cclk_line = strstr(response, "+CCLK:");
                            if (cclk_line) {
                                int year;
                                if (sscanf(cclk_line, "+CCLK: \"%2d", &year) == 1 && year >= 24) {
                                    freeReceivedMessage(response);
                                    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, 
                                            "Time synchronized successfully. Proceeding...");
                                    break;
                                }
                            }
                            freeReceivedMessage(response);
                        }
                        vTaskDelay(pdMS_TO_TICKS(2000));
                    }
                    
                    wait_for_valid_modem_time();
                    sync_esp32_time_with_modem();
                    SYNC_TIME=true;
                    modem_connect_data();
                    //esp_event_post(MODEM_EVENT, MODEM_EVENT_TIME_SYNCED, NULL, 0, portMAX_DELAY);
                    xEventGroupSetBits(modem_event_group, MODEM_DATA_CONNECTED_BIT);
                    
                    break;
                case CMD_START_GPS:
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Processing: Start GPS");
                         //Step 1: Power on GPS
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Powering on GPS...");
                        enable_gps_auto_start();
                        vTaskDelay(pdMS_TO_TICKS(2000));
                         //Step 2: Start GPS session with standalone mode
                        ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Starting GPS session...");
                        start_gps_session();
                        // Step 3: Get initial GPS info
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        get_gps_info();
                        modem_get_gps() ;
                        xEventGroupSetBits(modem_event_group, MODEM_DATA_CONNECTED_BIT);
                        break;
                case CMD_GET_GPS:
                         ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "GET GPS CORRDINATION...");
                         get_gps_info();
                         wait_for_gps_fix();
                         SYNC_GPS=true;
                         modem_connect_data();
                        break;
                case CMD_GET_SIGNAL:
                        signal_quality.signal_dbm  = get_signal_quality();
                        signal_quality.network_mode = get_network_mode();
                        break;                                   
                default:
                    ESP_LOGW(TAG_MODEM_EVENT_HANDLER, "Unknown command: %d", cmd.cmd);
                    break;
            }
            // Free command data if allocated
            if (cmd.data) {
                free(cmd.data);
            }
        }
    }
}
// Public function to initialize event system
esp_err_t modem_init(void) {
    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Initializing modem event system...");
    
    // Create event group
    modem_event_group = xEventGroupCreate();
    if (!modem_event_group) {
        ESP_LOGE(TAG_MODEM_EVENT_HANDLER, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Create command queue
    event_command_queue = xQueueCreate(10, sizeof(event_command_t));
    if (!event_command_queue) {
        ESP_LOGE(TAG_MODEM_EVENT_HANDLER, "Failed to create command queue");
        vEventGroupDelete(modem_event_group);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize UART and GPIO
   // gpio_init_control_SIMcom(powerkey, DTR);
        gpio_init_control_SIMcom(powerkey);
    UART_init_SIMcom();
    
    // Create AT response queue
    at_response_queue = xQueueCreate(20, sizeof(char*));
    
    // Create dispatcher task
    xTaskCreate(uart_dispatcher_task, "uart_dispatch", 4096, NULL, 10, NULL);
    
    // Create command processing task
    xTaskCreate(modem_command_task, "modem_cmd", 16384, NULL, 8, NULL);
    
    // Set MQTT message callback
    SIM7600_mqtt_set_message_callback(mqtt_message_handler);
    
    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Modem event system initialized");
    // Start the process

    ESP_LOGI(TAG_MODEM_EVENT_HANDLER, "Starting modem power-on sequence...");
    modem_power_on(); 
    return ESP_OK;
}

// Queue commands for execution
static esp_err_t queue_command(event_cmd_t cmd, void *data, size_t data_len) {
    if (!event_command_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    
    event_command_t cmd_struct = {
        .cmd = cmd,
        .data = NULL,
        .data_len = data_len
    };
    
    if (data && data_len > 0) {
        cmd_struct.data = malloc(data_len);
        if (!cmd_struct.data) {
            return ESP_ERR_NO_MEM;
        }
        memcpy(cmd_struct.data, data, data_len);
    }
    
    if (xQueueSend(event_command_queue, &cmd_struct, pdMS_TO_TICKS(1000)) != pdTRUE) {
        if (cmd_struct.data) free(cmd_struct.data);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
// Public API functions
esp_err_t modem_power_on(void) {
    return queue_command(CMD_POWER_ON, NULL, 0);
}
// Public API functions
esp_err_t modem_restart(void) {
    return queue_command(CMD_POWER_RESET, NULL, 0);
}


esp_err_t modem_initialize(void) {
    return queue_command(CMD_INIT_MODEM, NULL, 0);
}

esp_err_t modem_register_network(void) {
    return queue_command(CMD_REGISTER_NETWORK, NULL, 0);
}

esp_err_t modem_connect_data() {
    return  queue_command(CMD_CONNECT_DATA, NULL, 0);
}

esp_err_t modem_start_gps(void) {
    return queue_command(CMD_START_GPS, NULL, 0);
}

esp_err_t modem_get_gps(void) {
    return queue_command(CMD_GET_GPS, NULL, 0);
}

esp_err_t modem_sync_time(void) {
    return queue_command(CMD_SYNC_TIME, NULL, 0);
}
esp_err_t modem_get_signal(void) {
    return queue_command(CMD_GET_SIGNAL, NULL, 0);
}


// Wait for specific modem states
esp_err_t modem_wait_for_state(modem_state_t state, TickType_t timeout) {
    EventBits_t bits = 0;
    
    switch (state) {
        case MODEM_STATE_POWERED_ON:
            bits = MODEM_POWERED_ON;
            break;
        case MODEM_STATE_POWERED_RESET:
            bits = MODEM_POWERED_RESET;
            break;
        case MODEM_STATE_INIT:
            bits = MODEM_INIT_BIT;
            break;
        case MODEM_STATE_REGISTERED:
            bits = MODEM_REGISTERED_BIT;
            break;
        case MODEM_STATE_DATA_CONNECTED:
            bits = MODEM_DATA_CONNECTED_BIT;
            break;
        case MODEM_STATE_MQTT_CONNECTED:
            bits = MODEM_MQTT_CONNECTED_BIT;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    EventBits_t result = xEventGroupWaitBits(modem_event_group, bits, 
                                            pdFALSE, pdTRUE, timeout);
    
    return (result & bits) ? ESP_OK : ESP_ERR_TIMEOUT;
}