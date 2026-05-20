#include "SIM7600.h"
#include "SIMCOM_Driver.h"
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
static const char *TAG_SIM7600 = "SIM7600";
static SIM7600_mqtt_message_callback_t mqtt_msg_callback = NULL;

/* ---------- MAC address helpers ---------- */
static char s_device_mac_str[18] = {0};  /* "XX:XX:XX:XX:XX:XX\0" */

void read_and_store_mac(void) {
    uint8_t mac[6];
    /* esp_read_mac is defined in esp_mac.h (already included via SIM7600.h)
     * and is available across all ESP-IDF versions for ESP32. */
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_device_mac_str, sizeof(s_device_mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG_SIM7600, "Device MAC: %s", s_device_mac_str);
}

const char* get_device_mac_str(void) {
    if (s_device_mac_str[0] == '\0') {
        read_and_store_mac();   /* lazy-init: read once on first call */
    }
    return s_device_mac_str;
}
/* ---------------------------------------- */





void parse_gps_info(const char* response) {
    const char* prefix = "+CGPSINFO: ";
  
}




esp_err_t modemInit_SIM7600() {
    const TickType_t timeout = pdMS_TO_TICKS(120000);  // 120 seconds timeout
    TickType_t start_time = xTaskGetTickCount();


    while ((xTaskGetTickCount() - start_time) <= timeout) {
        flush_at_response_queue();
        UART_sendd("AT\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        bool got_ok = false;
        for (int i = 0; i < 5; i++) {
            char *received_message = UART_receive(500);
            if (received_message) {
                printf("modem init: %s\r\n", received_message);
                if (strstr(received_message, "OK")) got_ok = true;
                if (strstr(received_message, "ERROR")) {
                    freeReceivedMessage(received_message);
                    return ESP_FAIL;
                }
                freeReceivedMessage(received_message);
                if (got_ok) break;
            }
        }
        if (got_ok) {
            printf("modem AT Done\r\n");
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG_SIM7600, "Modem initialization timeout");
    return ESP_FAIL;
}
esp_err_t Registring_SIM7600() {
    uint8_t repeting = 30;
    for (uint8_t i = 0; i <= repeting; i++) {
        flush_at_response_queue();
        UART_sendd("AT+CREG?\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        bool registered = false;
        for (int j = 0; j < 5; j++) { // Read up to 5 lines per command
            char *received_message = UART_receive(500);
            if (received_message) {
                printf("Registeration of SIM : %s\r\n", received_message);
                if (strstr(received_message, "0,1")) {
                    freeReceivedMessage(received_message);
                    return ESP_OK;
                }
                freeReceivedMessage(received_message);
            }
        }
    }
    return ESP_FAIL;
}
void SIM7600_restart_modem_via_at(void) {
    flush_at_response_queue();
    UART_sendd("AT+CFUN=6\r\n");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for the modem to reset
    char* response = UART_receive(2000);
    printf("AT+CFUN=6 response: %s\n", response ? response : "(null)");
    freeReceivedMessage(response);
}
esp_err_t modemconfigmode_SIM7600(char *PreferredModeSelection,char *PreferredSelection,char *APN)
{
            char command[100];
            snprintf(command, sizeof(command), "AT+CNMP=%s\r\n", PreferredModeSelection); 
            flush_at_response_queue();
            UART_sendd(command);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            char *received_message = UART_receive(100);
            printf( "set Preferred Mode Selection: %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
           // freeReceivedMessage(command);

            flush_at_response_queue();
            UART_sendd("AT+CNBP=,0x0000000000000095\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "set Preferred Selection Band: %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
            //freeReceivedMessage(command);
            
            flush_at_response_queue();
            UART_sendd("AT+CNBP?\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "Get Preferred Selection Band: %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
          
            flush_at_response_queue();
            UART_sendd("AT+CNSMOD=1\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
             printf( "Get Network System Mode %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);

    
        
            flush_at_response_queue();
            UART_sendd("AT+CNSMOD?\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message =UART_receive(100);
            printf( "Get Network System Mode %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);

          
            flush_at_response_queue();
            UART_sendd(" AT+CGATT=1\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "Set the Mode of Application Configure APN %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
            
            flush_at_response_queue();
            UART_sendd(" AT+CGATT?\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "get the Mode of Application Configure APN %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
            
         const TickType_t timeout = pdMS_TO_TICKS(10000);  // 60 seconds timeout
         TickType_t start_time = xTaskGetTickCount();
         while(1)
         {    if ((xTaskGetTickCount() - start_time) > timeout) {
            ESP_LOGE(TAG_SIM7600, "Modem initialization timeout");
            return ESP_FAIL;  // Timeout exceeded
        }
          
            flush_at_response_queue();
            UART_sendd("AT+CGDCONT?\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message =UART_receive(100);
            printf( "get Define a PDP Context %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
                   
           snprintf(command, sizeof(command), "AT+CGDCONT=1,\"IP\",\"%s\"\r\n",APN); 
            flush_at_response_queue();
            UART_sendd(command);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "set Define a PDP Context %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);
     
            flush_at_response_queue();
            UART_sendd("AT+CGREG?\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message =UART_receive(100);
            printf( "Get PDP Configure %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);


            flush_at_response_queue();
            UART_sendd("AT+CGPADDR=1\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
            received_message = UART_receive(100);
            printf( "Get PDP Configure %s\r\n", received_message ? received_message : "(null)");
            freeReceivedMessage(received_message);


           if(strstr(received_message,"1,"))
           {
            return ESP_OK;
           }
         }
        return ESP_FAIL;
}
esp_err_t SIMcom_connect_SIM7600(char *APN){
    ESP_LOGI(TAG_SIM7600, "SIM7600 Connect \r\n");
    char *received_message = NULL;
    bool got_ok = false;

    // Set CGATT=1 (Attach to GPRS)
    flush_at_response_queue();
    UART_sendd("AT+CGATT=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        received_message = UART_receive(500);
        if (received_message) {
            printf("Set CGATT=1: %s\r\n", received_message);
            if (strstr(received_message, "OK")) got_ok = true;
            if (strstr(received_message, "ERROR")) {
                freeReceivedMessage(received_message);
                return ESP_FAIL;
            }
            freeReceivedMessage(received_message);
            if (got_ok) break;
        }
    }
    if (!got_ok) {
        printf("Failed to set CGATT=1\n");
        return ESP_FAIL;
    }
            
    // Check CGATT status
    flush_at_response_queue();
    UART_sendd("AT+CGATT?\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        received_message = UART_receive(500);
        if (received_message) {
            printf("Get CGATT status: %s\r\n", received_message);
            if (strstr(received_message, "OK")) got_ok = true;
            if (strstr(received_message, "ERROR")) {
                freeReceivedMessage(received_message);
                return ESP_FAIL;
            }
            freeReceivedMessage(received_message);
            if (got_ok) break;
        }
    }
    if (!got_ok) {
        printf("Failed to get CGATT status\n");
        return ESP_FAIL;
    }
            
    const TickType_t timeout = pdMS_TO_TICKS(30000);  // 30 seconds timeout
    TickType_t start_time = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_time) <= timeout) {
        // Get PDP context
        flush_at_response_queue();
        UART_sendd("AT+CGDCONT?\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        got_ok = false;
        for (int i = 0; i < 5; i++) {
            received_message = UART_receive(500);
            if (received_message) {
                printf("Get PDP Context: %s\r\n", received_message);
                if (strstr(received_message, "OK")) got_ok = true;
                if (strstr(received_message, "ERROR")) {
                    freeReceivedMessage(received_message);
                    return ESP_FAIL;
                }
                freeReceivedMessage(received_message);
                if (got_ok) break;
            }
        }
        if (!got_ok) {
            printf("Failed to get PDP context\n");
            return ESP_FAIL;
        }
                   
        // Set PDP context
        char command[100];     
        snprintf(command, sizeof(command), "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", APN); 
        flush_at_response_queue();
        UART_sendd(command);
        vTaskDelay(pdMS_TO_TICKS(1000));
        got_ok = false;
        for (int i = 0; i < 5; i++) {
            received_message = UART_receive(500);
            if (received_message) {
                printf("Set PDP Context: %s\r\n", received_message);
                if (strstr(received_message, "OK")) got_ok = true;
                if (strstr(received_message, "ERROR")) {
                    freeReceivedMessage(received_message);
                    return ESP_FAIL;
                }
                freeReceivedMessage(received_message);
                if (got_ok) break;
            }
        }
        if (!got_ok) {
            printf("Failed to set PDP context\n");
            return ESP_FAIL;
        }
     
        // Check GPRS registration
        flush_at_response_queue();
        UART_sendd("AT+CGREG?\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        got_ok = false;
        for (int i = 0; i < 5; i++) {
            received_message = UART_receive(500);
            if (received_message) {
                printf("Get GPRS registration: %s\r\n", received_message);
                if (strstr(received_message, "OK")) got_ok = true;
                if (strstr(received_message, "ERROR")) {
                    freeReceivedMessage(received_message);
                    return ESP_FAIL;
                }
                freeReceivedMessage(received_message);
                if (got_ok) break;
            }
        }
        if (!got_ok) {
            printf("Failed to get GPRS registration\n");
            return ESP_FAIL;
        }

        // Get PDP address
        flush_at_response_queue();
        UART_sendd("AT+CGPADDR=1\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        got_ok = false;
        for (int i = 0; i < 5; i++) {
            received_message = UART_receive(500);
            if (received_message) {
                printf("Get PDP address: %s\r\n", received_message);
                if (strstr(received_message, "OK")) got_ok = true;
                if (strstr(received_message, "ERROR")) {
                    freeReceivedMessage(received_message);
                    return ESP_FAIL;
                }
                freeReceivedMessage(received_message);
                if (got_ok) break;
            }
        }
        if (!got_ok) {
            printf("Failed to get PDP address\n");
            return ESP_FAIL;
        }

        // Check if we have an IP address (not 0.0.0.0)
        if (received_message && !strstr(received_message, "1,0")) {
            printf("GPRS connection established successfully!\n");
            return ESP_OK;
        }
        
        printf("Waiting for IP address...\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds before retry
    }
    
    ESP_LOGE(TAG_SIM7600, "GPRS connection timeout");
    return ESP_FAIL;
}

/***************** */
void get_unix_millis_timestamp(char* buf, size_t len) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long ms = (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    snprintf(buf, len, "%lld", ms);
}

void start_gps_session() {
    flush_at_response_queue();
    UART_sendd("AT+CGPS=1,1\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    bool got_ok = false;
    for (int i = 0; i < 5; i++) {
        char* response = UART_receive(500);
        if (response) {
            printf("Start GPS Session Response: %s\n", response);
            if (strstr(response, "OK")) {
                got_ok = true;
            }
            if (strstr(response, "ERROR")) {
                freeReceivedMessage(response);
                return;
            }
            freeReceivedMessage(response);
            if (got_ok) break;
        }
    }
    if (!got_ok) {
        printf("Failed to start GPS session: No OK received.\n");
    }
}
void get_gps_info() {
    flush_at_response_queue();
    UART_sendd("AT+CGPSINFO\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
    char* response = UART_receive(1000);
    printf("GPS Info Response: %s\n", response ? response : "(null)");
    parse_gps_info(response);
    freeReceivedMessage(response);
}
void enable_gps_auto_start() {
    flush_at_response_queue();
    UART_sendd("AT+CGPSAUTO=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
    char* response = UART_receive(500);
    printf("Enable GPS Auto Start Response: %s\n", response ? response : "(null)");
    freeReceivedMessage(response);
}
void wait_for_gps_fix() {
    while (1) {
        flush_at_response_queue();
        UART_sendd("AT+CGPSINFO\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        char* gpsinfo_line = NULL;
        // Read up to 5 lines to find the +CGPSINFO: response
        for (int i = 0; i < 5; i++) {
            char* response = UART_receive(500);
            if (response) {
                printf("GPS Info Response: %s\n", response);
                const char* prefix = "+CGPSINFO: ";
                const char* start = strstr(response, prefix);
                if (start) {
                    // Found the GPS info line
                    gpsinfo_line = response; // Don't free yet
                    break;
                }
                freeReceivedMessage(response);
            }
        }
        if (gpsinfo_line) {
            parse_gps_info(gpsinfo_line);
            // Check if the first field (lat) is not empty
            const char* prefix = "+CGPSINFO: ";
            const char* start = strstr(gpsinfo_line, prefix);
            if (start) {
                start += strlen(prefix);
                if (start[0] != ',' && start[0] != '\0') {
                    printf("GPS fix acquired!\n");
                    freeReceivedMessage(gpsinfo_line);
                    break;
                }
            }
            freeReceivedMessage(gpsinfo_line);
        } else {
            printf("No +CGPSINFO: line found in modem response.\n");
        }
    }
}
void test_gps_command_support() {
    flush_at_response_queue();
    UART_sendd("AT+CGPS=?\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Corrected: added semicolon and conversion
    char* response = UART_receive(500);
    printf("GPS Test Command Response: %s\n", response ? response : "(null)");
    freeReceivedMessage(response);
}
/*
esp_err_t SIM7600_connect_mqtt(const char* client_id, const char* broker_url, const char* will_topic, const char* will_msg, const char* sub_topic, const char* pub_topic, const char* pub_payload) {
    char cmd[128];
    char* resp;
    bool got_ok = false, got_start = false;

    // Start MQTT service
    printf("AT> AT+CMQTTSTART\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTSTART\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTSTART: 0")) got_start = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_start) break;
        }
    }
    if (!(got_ok && got_start)) return ESP_FAIL;

    // Configure MQTT UTF-8 checking
    printf("AT> AT+CMQTTCFG=\"checkUTF8\",0,0\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTCFG=\"checkUTF8\",0,0\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Acquire client
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"\r\n", client_id);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set MQTT Will Topic
    int will_topic_len = strlen(will_topic);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLTOPIC=0,%d\r\n", will_topic_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    bool got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send will topic
    printf("AT> %s\n", will_topic);
    flush_at_response_queue();
    UART_sendd(will_topic);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set MQTT Will Message
    int will_msg_len = strlen(will_msg);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLMSG=0,%d,1\r\n", will_msg_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send will message
    printf("AT> %s\n", will_msg);
    flush_at_response_queue();
    UART_sendd(will_msg);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Connect to MQTT server
    snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"%s\",60,1\r\n", broker_url);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    got_ok = false;
    bool got_connect_urc = false;
    for (int i = 0; i < 20; i++) { // Longer timeout for connection
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            freeReceivedMessage(resp);
        }
    }
    //if (!got_ok || !got_connect_urc) return ESP_FAIL;

    // Subscribe to topic
    int sub_topic_len = strlen(sub_topic);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,%d,1\r\n", sub_topic_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send subscribe topic
    printf("AT> %s\n", sub_topic);
    flush_at_response_queue();
    UART_sendd(sub_topic);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set publish topic
    int pub_topic_len = strlen(pub_topic);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d\r\n", pub_topic_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send publish topic
    printf("AT> %s\n", pub_topic);
    flush_at_response_queue();
    UART_sendd(pub_topic);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set publish payload
    int pub_payload_len = strlen(pub_payload);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d\r\n", pub_payload_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send publish payload
    printf("AT> %s\n", pub_payload);
    flush_at_response_queue();
    UART_sendd(pub_payload);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Publish message
    printf("AT> AT+CMQTTPUB=0,1,60\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTPUB=0,1,60\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    return ESP_OK;
}
esp_err_t SIM7600_mqtt_connect(const char* client_id) {
    char cmd[64];
    char* resp;
    bool got_ok = false, got_start = false;

    // Start MQTT service
    printf("AT> AT+CMQTTSTART\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTSTART\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTSTART: 0")) got_start = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_start) break;
        }
    }
    if (!(got_ok && got_start)) return ESP_FAIL;

    // Configure MQTT UTF-8 checking
    printf("AT> AT+CMQTTCFG=\"checkUTF8\",0,0\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTCFG=\"checkUTF8\",0,0\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Acquire client
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"\r\n", client_id);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set MQTT Will Topic — must match the platform status topic exactly
    const char* device_mac = get_device_mac_str();
    char will_topic_buf[128];
    snprintf(will_topic_buf, sizeof(will_topic_buf),
             "fuel/699700a5d383e0a593047e03/%s/status", device_mac);
    const char* will_topic = will_topic_buf;
    int will_topic_len = strlen(will_topic);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLTOPIC=0,%d\r\n", will_topic_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    bool got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send will topic
    printf("AT> %s\n", will_topic);
    flush_at_response_queue();
    UART_sendd(will_topic);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    // Set MQTT Will Message — "offline" is what the platform expects
    // QoS=1. NOTE: this SIM7600 firmware does not support the optional <retained>
    // 4th parameter — sending it causes an AT ERROR that aborts the LWT setup.
    const char* will_msg = "offline";
    int will_msg_len = strlen(will_msg);
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLMSG=0,%d,1\r\n", will_msg_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_prompt = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    // Send will message
    printf("AT> %s\n", will_msg);
    flush_at_response_queue();
    UART_sendd(will_msg);
    vTaskDelay(pdMS_TO_TICKS(500));
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    return ESP_OK;
}
esp_err_t SIM7600_mqtt_set_broker(const char* broker_url) {
    char cmd[128];
    char* resp;
    bool got_ok = false, got_connect = false;

    snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=0,\"%s\",60,1\r\n", broker_url);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Longer delay for connection

    // Wait for OK and +CMQTTCONNECT: 0,0
    for (int i = 0; i < 15; i++) { // More iterations for connection
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTCONNECT: 0,0")) got_connect = true;
            if (strstr(resp, "+CMQTTCONNECT: 0,1") || strstr(resp, "+CMQTTCONNECT: 0,2")) {
                // Connection failed
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_connect) break;
        }
    }
    if (!(got_ok && got_connect)) return ESP_FAIL;

    return ESP_OK;
}
esp_err_t SIM7600_mqtt_subscribe(const char* topic) {
    char cmd[64];
    char* resp;
    bool got_prompt = false, got_ok = false, got_sub = false;
    int topic_len = strlen(topic);

    printf("SIM7600_mqtt_subscribe to topic: '%s', length: %d\n", topic, topic_len);
    



    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=0,%d,1\r\n", topic_len);
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wait for prompt
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    
    // Send topic
    printf("AT> %s\n", topic);
    flush_at_response_queue();
    UART_sendd(topic);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wait for OK and +CMQTTSUB: 0,0
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTSUB: 0,0")) got_sub = true;
            if (strstr(resp, "+CMQTTSUB: 0,1") || strstr(resp, "+CMQTTSUB: 0,2")) {
                // Subscribe failed
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_sub) break;
        }
    }
    if (!(got_ok && got_sub)) return ESP_FAIL;
    
    return ESP_OK;
}
esp_err_t SIM7600_mqtt_unsubscribe(const char* topic) {
    char cmd[64];
    char* resp;
    bool got_prompt = false, got_ok = false, got_unsub = false;

    snprintf(cmd, sizeof(cmd), "AT+CMQTTUNSUB=0,%d,0\r\n", (int)strlen(topic));
    printf("AT> %s", cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wait for prompt
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strchr(resp, '>')) got_prompt = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_prompt) break;
        }
    }
    if (!got_prompt) return ESP_FAIL;
    
    // Send topic
    printf("AT> %s\n", topic);
    flush_at_response_queue();
    UART_sendd(topic);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wait for OK and +CMQTTUNSUB: 0,0
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTUNSUB: 0,0")) got_unsub = true;
            if (strstr(resp, "+CMQTTUNSUB: 0,1") || strstr(resp, "+CMQTTUNSUB: 0,2")) {
                // Unsubscribe failed
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_unsub) break;
        }
    }
    if (!(got_ok && got_unsub)) return ESP_FAIL;
    
    return ESP_OK;
}

esp_err_t SIM7600_mqtt_publish(const char* topic, const char* payload) {
    // Input validation
    if (topic == NULL || payload == NULL) {
        printf("SIM7600_mqtt_publish: Invalid arguments (topic or payload is NULL)\n");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create compact payload (remove newlines, carriage returns, and extra spaces)
    char *compact_payload = malloc(strlen(payload) + 1);
    if (compact_payload == NULL) {
        printf("SIM7600_mqtt_publish: Failed to allocate memory for compact payload\n");
        return ESP_ERR_NO_MEM;
    }
    
    // Remove whitespace characters for reliable transmission
    const char *src = payload;
    char *dst = compact_payload;
    while (*src) {
        if (*src != '\n' && *src != '\r' && *src != '\t') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    printf("SIM7600_mqtt_publish: Original payload length: %d, Compact payload length: %d\n", 
           strlen(payload), strlen(compact_payload));
    printf("SIM7600_mqtt_publish: Topic: %s, Compact payload: %s\n", topic, compact_payload);
    
    char cmd[64];
    char* resp;
    bool got_prompt = false, got_ok = false;
    int topic_len = strlen(topic);
    int payload_len = strlen(compact_payload);
    int retry_count = 0;
    const int max_retries = 3;
    
    // Set topic with retry mechanism
    for (retry_count = 0; retry_count < max_retries; retry_count++) {
        got_prompt = false;
        
        // Send AT command to set topic length
        snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d\r\n", topic_len);
        printf("AT> %s", cmd);
        flush_at_response_queue();
        UART_sendd(cmd);
        
        // Wait for prompt with timeout
        int prompt_timeout = 10; // Try up to 10 times (5 seconds total with 500ms each)
        for (int i = 0; i < prompt_timeout; i++) {
            resp = UART_receive(500); // 500ms timeout
            if (resp) {
                printf("AT< %s\n", resp);
                if (strchr(resp, '>')) {
                    got_prompt = true;
                    freeReceivedMessage(resp);
                    break;
                }
                if (strstr(resp, "ERROR") || strstr(resp, "error") || strstr(resp, "ERROR")) {
                    printf("SIM7600_mqtt_publish: Received ERROR response for CMQTTTOPIC\n");
                    freeReceivedMessage(resp);
                    break;
                }
                freeReceivedMessage(resp);
            }
        }
        
        if (got_prompt) {
            break;
        }
        
        printf("SIM7600_mqtt_publish: Failed to get prompt for topic, retrying... (%d/%d)\n", 
               retry_count + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (!got_prompt) {
        printf("SIM7600_mqtt_publish: Failed to get prompt for topic after %d retries\n", max_retries);
        free(compact_payload);
        return ESP_FAIL;
    }
    
    // Send the actual topic
    printf("AT> %s\n", topic);
    flush_at_response_queue();
    UART_sendd(topic);
    
    // Wait for OK response
    got_ok = false;
    for (int i = 0; i < 10; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) {
                got_ok = true;
                freeReceivedMessage(resp);
                break;
            }
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                printf("SIM7600_mqtt_publish: ERROR response for topic\n");
                freeReceivedMessage(resp);
                free(compact_payload);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
        }
    }
    
    if (!got_ok) {
        printf("SIM7600_mqtt_publish: No OK response for topic\n");
        free(compact_payload);
        return ESP_FAIL;
    }
    
    // Small delay between commands
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Set payload length
    for (retry_count = 0; retry_count < max_retries; retry_count++) {
        got_prompt = false;
        
        snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d\r\n", payload_len);
        printf("AT> %s", cmd);
        flush_at_response_queue();
        UART_sendd(cmd);
        
        // Wait for prompt
        for (int i = 0; i < 10; i++) {
            resp = UART_receive(500);
            if (resp) {
                printf("AT< %s\n", resp);
                if (strchr(resp, '>')) {
                    got_prompt = true;
                    freeReceivedMessage(resp);
                    break;
                }
                if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                    printf("SIM7600_mqtt_publish: ERROR response for CMQTTPAYLOAD\n");
                    freeReceivedMessage(resp);
                    break;
                }
                freeReceivedMessage(resp);
            }
        }
        
        if (got_prompt) {
            break;
        }
        
        printf("SIM7600_mqtt_publish: Failed to get prompt for payload, retrying... (%d/%d)\n", 
               retry_count + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (!got_prompt) {
        printf("SIM7600_mqtt_publish: Failed to get prompt for payload after %d retries\n", max_retries);
        free(compact_payload);
        return ESP_FAIL;
    }
    
    // Send the compact payload
    printf("AT> %s\n", compact_payload);
    flush_at_response_queue();
    
    // Send payload in one go - important to send as a single block
    UART_sendd(compact_payload);
    
    // Wait for OK response
    got_ok = false;
    for (int i = 0; i < 10; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) {
                got_ok = true;
                freeReceivedMessage(resp);
                break;
            }
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                printf("SIM7600_mqtt_publish: ERROR response for payload\n");
                freeReceivedMessage(resp);
                free(compact_payload);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
        }
    }
    
    if (!got_ok) {
        printf("SIM7600_mqtt_publish: No OK response for payload\n");
        free(compact_payload);
        return ESP_FAIL;
    }
    
    // Small delay before publish
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Publish the message
    printf("AT> AT+CMQTTPUB=0,1,10,1\r\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTPUB=0,1,10,1\r\n");
    
    // Wait for both OK and +CMQTTPUB: 0,0 response
    got_ok = false;
    bool got_pub_success = false;
    
    for (int i = 0; i < 15; i++) { // Try up to 15 times (7.5 seconds total)
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            
            if (strstr(resp, "OK")) {
                got_ok = true;
            }
            
            /* Parse the result code properly — do NOT use strstr() here because
             * "+CMQTTPUB: 0,1" is a substring of "+CMQTTPUB: 0,12", causing every
             * code-12 response to be falsely treated as a code-1 failure.       */
            int pub_result_code = -1;
            if (sscanf(resp, "+CMQTTPUB: 0,%d", &pub_result_code) == 1) {
                if (pub_result_code == 0) {
                    got_pub_success = true;
                    printf("SIM7600_mqtt_publish: Publish successful\n");
                } else {
                    printf("SIM7600_mqtt_publish: Publish failed with code %d\n",
                           pub_result_code);
                    freeReceivedMessage(resp);
                    free(compact_payload);
                    return ESP_FAIL;
                }
            }
            
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                printf("SIM7600_mqtt_publish: ERROR response for publish\n");
                freeReceivedMessage(resp);
                free(compact_payload);
                return ESP_FAIL;
            }
            
            freeReceivedMessage(resp);
            
            // If we have both OK and success, we're done
            if (got_ok && got_pub_success) {
                break;
            }
        }
    }
    
    // Clean up
    free(compact_payload);
    
    if (!got_ok || !got_pub_success) {
        printf("SIM7600_mqtt_publish: Publish incomplete - OK: %d, Success: %d\n", 
               got_ok, got_pub_success);
        return ESP_FAIL;
    }
    
    printf("SIM7600_mqtt_publish: Message published successfully\n");
    return ESP_OK;
}

esp_err_t SIM7600_mqtt_disconnect(void) {
    char* resp;
    bool got_ok = false, got_disc = false;

    printf("AT> AT+CMQTTDISC=0,120\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTDISC=0,120\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Wait for OK and +CMQTTDISC: 0,0
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTDISC: 0,0")) got_disc = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_disc) break;
        }
    }
    if (!(got_ok && got_disc)) return ESP_FAIL;

    printf("AT> AT+CMQTTREL=0\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTREL=0\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    got_ok = false;
    for (int i = 0; i < 5; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok) break;
        }
    }
    if (!got_ok) return ESP_FAIL;

    printf("AT> AT+CMQTTSTOP\n");
    flush_at_response_queue();
    UART_sendd("AT+CMQTTSTOP\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    got_ok = false;
    bool got_stop = false;
    for (int i = 0; i < 7; i++) {
        resp = UART_receive(500);
        if (resp) {
            printf("AT< %s\n", resp);
            if (strstr(resp, "OK")) got_ok = true;
            if (strstr(resp, "+CMQTTSTOP: 0")) got_stop = true;
            if (strstr(resp, "ERROR")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            freeReceivedMessage(resp);
            if (got_ok && got_stop) break;
        }
    }
    if (!(got_ok && got_stop)) return ESP_FAIL;
    
    return ESP_OK;
}
void SIM7600_mqtt_set_message_callback(SIM7600_mqtt_message_callback_t callback) {
    mqtt_msg_callback = callback;
    printf("[SIM7600] MQTT message callback set\n");
}
*/
char* get_modem_time_response() {
    UART_sendd("AT+CCLK?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    for (int i = 0; i < 5; i++) {
        char* line = UART_receive(500);
        if (line) {
            if (strstr(line, "+CCLK:")) {
                // Return this line to caller (caller must free)
                return line;
            }
            // Free lines that are not the response
            freeReceivedMessage(line);
        }
    }
    return NULL;
}
void wait_for_valid_modem_time() {
    while (1) {
        char* response = get_modem_time_response();
        if (response) {
            printf("modem time: %s\n", response);
            char* cclk_line = strstr(response, "+CCLK:");
            if (cclk_line) {
                int year;
                if (sscanf(cclk_line, "+CCLK: \"%2d", &year) == 1 && year >= 24) {
                    freeReceivedMessage(response);
                    printf("Time synchronized successfully. Proceeding to enable GPS and MQTT.\n");
                    break;
                }
            }
            freeReceivedMessage(response);
        } else {
            printf("Failed to get modem time response.\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds before retry
    }
}   
void sync_esp32_time_with_modem() {
    char* response = get_modem_time_response();
    if (!response) return;

    char* cclk_line = strstr(response, "+CCLK:");
    if (cclk_line) {
        int year, month, day, hour, min, sec, tz;
        if (sscanf(cclk_line, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%2d",
                    &year, &month, &day, &hour, &min, &sec, &tz) == 7) {
            
            // The time from modem is already in local timezone
            // We need to convert it to UTC for ESP32
            struct tm t;
            t.tm_year = 2000 + year - 1900; // years since 1900
            t.tm_mon = month - 1;
            t.tm_mday = day;
            t.tm_hour = hour;
            t.tm_min = min;
            t.tm_sec = sec;
            t.tm_isdst = -1;

            // Convert local time to UTC by subtracting timezone offset
            time_t local_epoch = mktime(&t);
            if (local_epoch > 0) {
                // Subtract timezone offset to get UTC time
                time_t utc_epoch = local_epoch - (tz * 3600); // tz is in hours
                
                struct timeval tv = { .tv_sec = utc_epoch, .tv_usec = 0 };
                settimeofday(&tv, NULL);
                
                // Print both local and UTC times for verification
                struct tm local_tm, utc_tm;
                localtime_r(&local_epoch, &local_tm);
                gmtime_r(&utc_epoch, &utc_tm);
                
                printf("ESP32 RTC synchronized to modem time:\n");
                printf("  Local time (modem): %s", asctime(&local_tm));
                printf("  UTC time (ESP32): %s", asctime(&utc_tm));
                printf("  Timezone offset: UTC%+d\n", tz);
            }
        }
    }
    freeReceivedMessage(response);
}  
void enable_modem_time_auto_update() {
    flush_at_response_queue();
    UART_sendd("AT+CTZU=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    char* resp1 = UART_receive(500);
    printf("AT+CTZU=1 response: %s\n", resp1 ? resp1 : "(null)");
    freeReceivedMessage(resp1);

    flush_at_response_queue();
    UART_sendd("AT+CTZU?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    char* resp = UART_receive(500);
    printf("AT+CTZU? response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);
}
void enable_modem_time_zone_reporting() {
    flush_at_response_queue();
    UART_sendd("AT+CTZR=1\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    char* resp1 = UART_receive(500);
    printf("AT+CTZR=1 response: %s\n", resp1 ? resp1 : "(null)");
    freeReceivedMessage(resp1);

    flush_at_response_queue();
    UART_sendd("AT+CTZR?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    char* resp = UART_receive(500);
    printf("AT+CTZR? response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);

}
void set_esp32_timezone(int timezone_offset) {
    char tz_string[32];
    snprintf(tz_string, sizeof(tz_string), "UTC%+d", timezone_offset);
    setenv("TZ", tz_string, 1);
    tzset();
    printf("ESP32 timezone set to: %s\n", tz_string);
}
void sync_modem_time_with_ntp(const char* ntp_server, int timezone) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",%d\r\n", ntp_server, timezone);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    char* resp = UART_receive(1000);
    printf("AT+CNTP set server response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);

    flush_at_response_queue();
    UART_sendd("AT+CNTP\r\n");
    vTaskDelay(pdMS_TO_TICKS(5000)); // NTP sync may take a few seconds
    resp = UART_receive(2000);
    printf("AT+CNTP sync response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);

    flush_at_response_queue();
    UART_sendd("AT+CCLK?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    resp = UART_receive(500);
    printf("AT+CCLK? after NTP: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);
}






