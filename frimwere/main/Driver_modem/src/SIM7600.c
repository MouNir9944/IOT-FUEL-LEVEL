#include "SIM7600.h"
#include "SIMCOM_Driver.h"
#include "SIM7600_MQTT.h"
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include "nvs_Manager.h"
static const char *TAG_SIM7600 = "SIM7600";
/* ---------- MAC address helpers ---------- */
static char s_device_mac_str[18] = {0};  /* "XX:XX:XX:XX:XX:XX\0" */
// Global variable to store GPS coordinates
gps_coordinates_t current_gps = {0};

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





// Optional: Helper function to get the current GPS coordinates
gps_coordinates_t get_current_gps_coordinates(void) {
    return current_gps;
}


// In your get_signal_quality function, make sure it returns dBm:
int get_signal_quality(void) {
    flush_at_response_queue();
    UART_sendd("AT+CSQ\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    char* response = UART_receive(1000);
    int rssi_dbm = -1;
    
    if (response) {
        printf("Signal Quality Response: %s\r\n", response);
        char* csq_ptr = strstr(response, "+CSQ: ");
        if (csq_ptr) {
            csq_ptr += strlen("+CSQ: ");
            int csq_index = atoi(csq_ptr);  // This gives you 28
            
            // Convert CSQ index to RSSI in dBm
            if (csq_index == 0) {
                rssi_dbm = -115;
            } else if (csq_index == 1) {
                rssi_dbm = -111;
            } else if (csq_index >= 2 && csq_index <= 30) {
                rssi_dbm = -113 + (2 * csq_index);  // For 28: -113 + (2*28) = -57
            } else if (csq_index == 31) {
                rssi_dbm = -51;
            } else if (csq_index == 99) {
                rssi_dbm = -1;
            }
            
            printf("CSQ Index: %d, Converted RSSI: %d dBm\r\n", csq_index, rssi_dbm);
        }
        freeReceivedMessage(response);
    }
    
    return rssi_dbm;  // Returns -57 for CSQ index 28
}

/* ── AT+CNSMOD? — return the <stat> field (0–16) or -1 on failure ──────────── *
 *
 * Two possible response formats because AT+CNSMOD=1 enables auto-reporting:
 *
 *   Read command response:  +CNSMOD: <n>,<stat>   e.g.  "+CNSMOD: 1,8"
 *   Unsolicited URC:        +CNSMOD: <stat>        e.g.  "+CNSMOD: 8"
 *
 * We detect which format by checking for a comma after the first number.
 * ─────────────────────────────────────────────────────────────────────────── */
int get_network_mode(void) {
    flush_at_response_queue();
    UART_sendd("AT+CNSMOD?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));

    int stat = -1;

    /* Poll up to 3 frames — the +CNSMOD line may not be the first in the queue */
    for (int t = 0; t < 3 && stat == -1; t++) {
        char *response = UART_receive(1000);
        if (!response) continue;

        char *ptr = strstr(response, "+CNSMOD:");
        if (ptr) {
            ptr += strlen("+CNSMOD:");
            while (*ptr == ' ') ptr++;          /* skip optional space */

            char *comma = strchr(ptr, ',');
            if (comma) {
                /* Read-command format: +CNSMOD: <n>,<stat> — value is after comma */
                stat = atoi(comma + 1);
            } else {
                /* URC format: +CNSMOD: <stat> — value is the only number */
                stat = atoi(ptr);
            }
            if (stat < 0 || stat > 16) stat = -1;  /* sanity */
        }
        freeReceivedMessage(response);
        printf("network_mode : %s \r\n",network_mode_to_str(stat));
    }

    return stat;
}

/* Convert the CNSMOD <stat> integer to a short human-readable string */
const char *network_mode_to_str(int stat) {
    switch (stat) {
        case  0: return "No Service";
        case  1: return "GSM";
        case  2: return "GPRS";
        case  3: return "EDGE";
        case  4: return "WCDMA";
        case  5: return "HSDPA";
        case  6: return "HSUPA";
        case  7: return "HSPA";
        case  8: return "LTE";
        case  9: return "TDS-CDMA";
        case 10: return "TDS-HSDPA";
        case 11: return "TDS-HSUPA";
        case 12: return "TDS-HSPA";
        case 13: return "CDMA";
        case 14: return "EVDO";
        case 15: return "CDMA+EVDO";
        case 16: return "CDMA+LTE";
        default: return "Unknown";
    }
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
    /* +CREG: <n>,<stat>  — stat values:
     *   0  not registered, NOT searching  → SIM absent / dead → restart fast
     *   1  registered, home network       → success
     *   2  not registered, SEARCHING      → normal, but restart if stuck too long
     *   3  registration denied
     *   5  registered, roaming            → also success                       */
    const uint8_t MAX_ATTEMPTS      = 30; /* absolute poll limit               */
    const uint8_t NOT_SEARCH_LIMIT  =  5; /* consecutive 0,0 before restart    */
    const uint8_t SEARCHING_LIMIT   = 15; /* consecutive 0,2 before restart    */

    uint8_t not_searching_count = 0;   /* consecutive "not searching" (0,0)  */
    uint8_t searching_count     = 0;   /* consecutive "searching"    (0,2)   */

    for (uint8_t i = 0; i <= MAX_ATTEMPTS; i++) {
        flush_at_response_queue();
        UART_sendd("AT+CREG?\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));

        bool got_zero_zero = false;
        bool got_zero_two  = false;

        for (int j = 0; j < 5; j++) {   /* read up to 5 lines per poll */
            char *received_message = UART_receive(500);
            if (received_message) {
                printf("Registeration of SIM : %s\r\n", received_message);

                /* ── Registered (home or roaming) ─────────────────── */
                if (strstr(received_message, "0,1") ||
                    strstr(received_message, "0,5")) {
                    freeReceivedMessage(received_message);
                    return ESP_OK;
                }

                /* ── Not registered, NOT searching (SIM absent/dead) ─ */
                if (strstr(received_message, "0,0")) {
                    got_zero_zero = true;
                }

                /* ── Searching but stuck ───────────────────────────── */
                if (strstr(received_message, "0,2")) {
                    got_zero_two = true;
                }

                freeReceivedMessage(received_message);
            }
        }

        /* ── Evaluate this poll's result ──────────────────────────────── */
        if (got_zero_zero) {
            not_searching_count++;
            searching_count = 0;   /* reset the other counter */
            ESP_LOGW("SIM7600",
                     "Not registered, not searching (%d/%d) — SIM may be absent",
                     not_searching_count, NOT_SEARCH_LIMIT);

            if (not_searching_count >= NOT_SEARCH_LIMIT) {
                ESP_LOGE("SIM7600",
                         "SIM absent/dead after %d polls — restarting modem (AT+CFUN=1,1)",
                         NOT_SEARCH_LIMIT);
                flush_at_response_queue();
                UART_sendd("AT+CFUN=1,1\r\n");
                vTaskDelay(pdMS_TO_TICKS(10000));
                return ESP_FAIL;
            }

        } else if (got_zero_two) {
            searching_count++;
            not_searching_count = 0;   /* reset the other counter */
            ESP_LOGW("SIM7600",
                     "Searching for network (%d/%d) — no signal yet",
                     searching_count, SEARCHING_LIMIT);

            if (searching_count >= SEARCHING_LIMIT) {
                ESP_LOGE("SIM7600",
                         "Stuck searching after %d polls — restarting modem (AT+CFUN=1,1)",
                         SEARCHING_LIMIT);
                flush_at_response_queue();
                UART_sendd("AT+CFUN=1,1\r\n");
                vTaskDelay(pdMS_TO_TICKS(10000));
                return ESP_FAIL;
            }

        } else {
            /* Any other response (denied, unknown, etc.) — reset both counters */
            not_searching_count = 0;
            searching_count     = 0;
        }
    }

    ESP_LOGE("SIM7600", "Registration timeout after %d attempts", MAX_ATTEMPTS);
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
            vTaskDelay(pdMS_TO_TICKS(1000));
            received_message = UART_receive(100);
            printf("Get PDP Configure %s\r\n", received_message ? received_message : "(null)");

            /* Check BEFORE freeing — received_message is NULL after free() */
            bool has_ip = (received_message && strstr(received_message, "1,") != NULL);
            freeReceivedMessage(received_message);
            received_message = NULL;

            if (has_ip) {
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
    // First, check if GPS is already running
    flush_at_response_queue();
    UART_sendd("AT+CGPS?\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    bool gps_already_running = false;
    
    // Read response to AT+CGPS?
    for (int i = 0; i < 5; i++) {
        char* response = UART_receive(500);
        if (response) {
            printf("GPS Status Response: %s\n", response);
            
            // Check if GPS is already running
            // Response format: +CGPS: <state>
            // <state> = 1 means GPS is running
            if (strstr(response, "+CGPS: 1")) {
                gps_already_running = true;
                printf("GPS session is already running.\n");
                freeReceivedMessage(response);
                break;
            }
            // Check if GPS is off
            else if (strstr(response, "+CGPS: 0")) {
                printf("GPS is currently off.\n");
                freeReceivedMessage(response);
                break;
            }
            
            freeReceivedMessage(response);
        }
    }
    
    // If GPS is already running, no need to start again
    if (gps_already_running) {
        return;
    }
    
    // Start GPS session only if it's not already running
    printf("Starting GPS session...\n");
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
                printf("GPS session started successfully.\n");
            }
            else if (strstr(response, "ERROR")) {
                printf("Failed to start GPS session: ERROR received.\n");
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
void parse_gps_info(const char* gpsinfo_line) {
    const char* prefix = "+CGPSINFO: ";
    const char* start = strstr(gpsinfo_line, prefix);
    
    if (!start) {
        printf("Invalid GPS info line\n");
        return;
    }
    
    start += strlen(prefix);
    
    // Parse latitude (format: DDMM.MMMMM)
    char lat_str[20] = {0};
    char lat_dir;
    char lon_str[20] = {0};
    char lon_dir;
    int date;
    float time_val;
    float altitude;
    float speed;
    
    // Use sscanf to parse the CSV format
    int parsed = sscanf(start, "%[^,],%c,%[^,],%c,%d,%f,%f,%f",
                        lat_str, &lat_dir, lon_str, &lon_dir, 
                        &date, &time_val, &altitude, &speed);
    
    if (parsed >= 8) {
        // Convert latitude from DDMM.MMMMM to decimal degrees
        double lat_deg = 0;
        double lat_min = 0;
        
        // Parse latitude string (format: DDMM.MMMMM)
        char deg_str[3] = {0};
        strncpy(deg_str, lat_str, 2);
        deg_str[2] = '\0';
        lat_deg = atoi(deg_str);
        
        char min_str[20] = {0};
        strcpy(min_str, lat_str + 2);
        lat_min = atof(min_str);
        
        current_gps.latitude = lat_deg + (lat_min / 60.0);
        if (lat_dir == 'S') {
            current_gps.latitude = -current_gps.latitude;
        }
        
        // Convert longitude from DDDMM.MMMMM to decimal degrees
        double lon_deg = 0;
        double lon_min = 0;
        
        // Parse longitude string (format: DDDMM.MMMMM)
        char lon_deg_str[4] = {0};
        strncpy(lon_deg_str, lon_str, 3);
        lon_deg_str[3] = '\0';
        lon_deg = atoi(lon_deg_str);
        
        char lon_min_str[20] = {0};
        strcpy(lon_min_str, lon_str + 3);
        lon_min = atof(lon_min_str);
        
        current_gps.longitude = lon_deg + (lon_min / 60.0);
        if (lon_dir == 'W') {
            current_gps.longitude = -current_gps.longitude;
        }
        
        // Save other fields
        current_gps.lat_direction = lat_dir;
        current_gps.lon_direction = lon_dir;
        current_gps.date = date;
        current_gps.time = time_val;
        current_gps.altitude = altitude;
        current_gps.speed = speed;
        
        printf("GPS Parsed Successfully:\n");
        printf("  Latitude: %.6f %c\n", current_gps.latitude, lat_dir);
        printf("  Longitude: %.6f %c\n", current_gps.longitude, lon_dir);
        printf("  Date: %d\n", date);
        printf("  Time: %.1f\n", time_val);
        printf("  Altitude: %.1f m\n", altitude);
        printf("  Speed: %.1f knots\n", speed);
    } else {
        printf("Failed to parse GPS info. Parsed %d fields\n", parsed);
    }
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
            
            // Check if we have valid data (latitude not empty)
            const char* prefix = "+CGPSINFO: ";
            const char* start = strstr(gpsinfo_line, prefix);
            if (start) {
                start += strlen(prefix);
                if (start[0] != ',' && start[0] != '\0') {
                    printf("GPS fix acquired and saved!\n");
                    printf("Current GPS coordinates:\n");
                    printf("  Lat: %.6f, Lon: %.6f\n", 
                           current_gps.latitude, current_gps.longitude);
                                               // Save GPS coordinates to NVS
                    char lat_str[32];
                    char lon_str[32];
                    
                    // Convert float values to strings
                    snprintf(lat_str, sizeof(lat_str), "%.6f", current_gps.latitude);
                    snprintf(lon_str, sizeof(lon_str), "%.6f", current_gps.longitude);
                    
                    // Save to NVS
                    esp_err_t ret1 = saved_data_in_flash("gps_data", "latitude", lat_str);
                    esp_err_t ret2 = saved_data_in_flash("gps_data", "longitude", lon_str);
                    
                    if (ret1 == ESP_OK && ret2 == ESP_OK) {
                        printf("GPS coordinates saved to NVS successfully!\n");
                    } else {
                        printf("Failed to save GPS coordinates to NVS\n");
                    }
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



// Define constants for clarity
#define TIMEZONE_TUNISIA_UTC_P1 1  // Tunisia is UTC+1
#define QUARTERS_PER_HOUR 4

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
    if (!response) {
        ESP_LOGE(TAG_SIM7600, "Failed to get modem time response");
        return;
    }

    char* cclk_line = strstr(response, "+CCLK:");
    if (cclk_line) {
        int year, month, day, hour, min, sec;
        int tz_hour = 0;  // Initialize timezone
        char tz_sign = '+';
        
        // Parse the modem time string
        // Format: "+CCLK: "yy/MM/dd,hh:mm:ss+zz" (where zz is timezone in quarter-hours)
        int parsed = sscanf(cclk_line, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d%c%2d",
                           &year, &month, &day, &hour, &min, &sec, &tz_sign, &tz_hour);
        
        if (parsed >= 7) {  // At least year through timezone
            // Convert timezone from quarter-hours to hours
            int tz_offset_hours =0;// tz_hour / QUARTERS_PER_HOUR;
           // if (tz_sign == '-') {
           //     tz_offset_hours = -tz_offset_hours;
            //}
            
            // The modem time is in local time (already adjusted for timezone)
            struct tm tm_local = {0};
            tm_local.tm_year = 2000 + year - 1900;  // years since 1900
            tm_local.tm_mon = month - 1;
            tm_local.tm_mday = day;
            tm_local.tm_hour = hour;
            tm_local.tm_min = min;
            tm_local.tm_sec = sec;
            tm_local.tm_isdst = -1;  // Auto-determine DST

            // Convert local time to UTC for ESP32 RTC
            time_t local_epoch = mktime(&tm_local);
            if (local_epoch != -1) {
                // Convert local to UTC by subtracting timezone offset
                //time_t utc_epoch = local_epoch - (tz_offset_hours * 3600);
                time_t utc_epoch = local_epoch ;
                // Set ESP32 RTC to UTC
                struct timeval tv = { .tv_sec = utc_epoch, .tv_usec = 0 };
                settimeofday(&tv, NULL);
                
                // Set timezone for local time display
                //set_esp32_timezone(tz_offset_hours);
                
                // Verify the times
                struct tm tm_utc, tm_local_check;
                localtime_r(&utc_epoch, &tm_local_check);  // Should now show local time due to TZ setting
                gmtime_r(&utc_epoch, &tm_utc);
                
                ESP_LOGI(TAG_SIM7600, "ESP32 RTC synchronized:");
                ESP_LOGI(TAG_SIM7600, "  Modem local time: %02d/%02d/%02d %02d:%02d:%02d UTC%c%d", 
                        year, month, day, hour, min, sec, tz_sign, tz_offset_hours);
                ESP_LOGI(TAG_SIM7600, "  ESP32 UTC time: %04d-%02d-%02d %02d:%02d:%02d",
                        tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                        tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
                ESP_LOGI(TAG_SIM7600, "  ESP32 local time: %04d-%02d-%02d %02d:%02d:%02d",
                        tm_local_check.tm_year + 1900, tm_local_check.tm_mon + 1, tm_local_check.tm_mday,
                        tm_local_check.tm_hour, tm_local_check.tm_min, tm_local_check.tm_sec);
            } else {
                ESP_LOGE(TAG_SIM7600, "Failed to convert to epoch time");
            }
        } else {
            ESP_LOGE(TAG_SIM7600, "Failed to parse modem time string: %s", cclk_line);
            ESP_LOGI(TAG_SIM7600, "Expected format: +CCLK: \"yy/MM/dd,hh:mm:ss±zz\"");
        }
    }
    freeReceivedMessage(response);
} 
void enable_modem_time_auto_update() {
    flush_at_response_queue();
    UART_sendd("AT+CTZU=0\r\n");
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
    UART_sendd("AT+CTZR=0\r\n");
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
void set_esp32_timezone(int timezone_offset_hours) {
    char tz_string[64];
    
    // POSIX timezone string format: "STDoffset[DST]"
    // For simple fixed offset, we can use a custom format
    if (timezone_offset_hours >= 0) {
        // For UTC+1 (Tunisia), we want local time = UTC + 1
        // In POSIX, this is "UTC-1" (yes, it's reversed!)
        snprintf(tz_string, sizeof(tz_string), "UTC-%d", timezone_offset_hours);
    } else {
        snprintf(tz_string, sizeof(tz_string), "UTC+%d", -timezone_offset_hours);
    }
    
    setenv("TZ", tz_string, 1);
    tzset();
    
    ESP_LOGI(TAG_SIM7600, "ESP32 timezone set to: %s (offset: %+d hours)", 
             tz_string, timezone_offset_hours);
}
void sync_modem_time_with_ntp(const char* ntp_server, int timezone_hours) {
    char cmd[64];
    
    // Convert hours to quarters (most modems expect quarter-hours)
    // For UTC+1, this will be 4
    int timezone_quarters = timezone_hours * QUARTERS_PER_HOUR;
    
    // First, ensure timezone auto-update is enabled
    enable_modem_time_auto_update();
    enable_modem_time_zone_reporting();
    
    // Set NTP server with timezone in quarter-hours
    snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",%d\r\n", ntp_server, timezone_quarters);
    flush_at_response_queue();
    UART_sendd(cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
    char* resp = UART_receive(1000);
    printf("AT+CNTP set server response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);

    // Trigger NTP sync
    flush_at_response_queue();
    UART_sendd("AT+CNTP\r\n");
    vTaskDelay(pdMS_TO_TICKS(5000)); // NTP sync may take a few seconds
    resp = UART_receive(2000);
    printf("AT+CNTP sync response: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);

    // Verify the time
    flush_at_response_queue();
    UART_sendd("AT+CCLK?\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    resp = UART_receive(500);
    printf("AT+CCLK? after NTP: %s\n", resp ? resp : "(null)");
    freeReceivedMessage(resp);
}

/*===========================================================================
 * Simplified MQTT API — wrappers around SIM7600_MQTT driver
 * Manages a single internal client instance for ease of use.
 *=========================================================================*/

static sim7600_mqtt_client_t s_mqtt_client = {0};
static SIM7600_mqtt_message_callback_t s_user_msg_callback = NULL;

/* Bridge: sim7600_mqtt callback signature -> SIM7600 callback signature */
static void internal_mqtt_msg_bridge(uint8_t client_index,
                                     const char* topic,
                                     const char* payload,
                                     size_t payload_len) {
    (void)client_index;
    (void)payload_len;
    if (s_user_msg_callback) {
        s_user_msg_callback(topic, payload);
    }
}

void SIM7600_mqtt_set_message_callback(SIM7600_mqtt_message_callback_t callback) {
    s_user_msg_callback = callback;
    sim7600_mqtt_register_message_callback(internal_mqtt_msg_bridge);
}

esp_err_t SIM7600_mqtt_connect(const char* client_id) {
    sim7600_mqtt_init();
    memset(&s_mqtt_client, 0, sizeof(s_mqtt_client));
    s_mqtt_client.client_index = 0;

    /* Start the MQTT service (AT+CMQTTSTART) */
    esp_err_t ret = sim7600_mqtt_start(&s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SIM7600, "SIM7600_mqtt_connect: MQTT start failed");
        return ret;
    }

    /* Acquire a client slot (AT+CMQTTACCQ) */
    return sim7600_mqtt_client_acquire(&s_mqtt_client, client_id, false);
}

esp_err_t SIM7600_mqtt_set_will(const char* topic, const char* message, uint8_t qos) {
    /* Must be called after SIM7600_mqtt_connect() (AT+CMQTTACCQ) and
     * before SIM7600_mqtt_set_broker() (AT+CMQTTCONNECT). */
    return sim7600_mqtt_set_will(&s_mqtt_client, topic, message, qos);
}

esp_err_t SIM7600_mqtt_set_broker(const char* url) {
    /* Parse URL of the form "tcp://host:port" or "ssl://host:port" */
    char host[241] = {0};
    uint16_t port = 1883;

    const char* p = url;
    if (strncmp(p, "tcp://", 6) == 0)      p += 6;
    else if (strncmp(p, "ssl://", 6) == 0) p += 6;

    const char* colon = strrchr(p, ':');
    if (colon) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        strncpy(host, p, host_len);
        host[host_len] = '\0';
        port = (uint16_t)atoi(colon + 1);
    } else {
        strncpy(host, p, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }

    /* Connect to broker (AT+CMQTTCONNECT) */
    return sim7600_mqtt_connect(&s_mqtt_client, host, port, 60, 1, NULL, NULL);
}

esp_err_t SIM7600_mqtt_disconnect(void) {
    /* Save state before any sub-call clears the struct */
    sim7600_mqtt_state_t saved_state = s_mqtt_client.state;

    if (saved_state >= MQTT_STATE_CONNECTED) {
        sim7600_mqtt_disconnect_broker(&s_mqtt_client, 60);
    }
    if (saved_state >= MQTT_STATE_CLIENT_ACQUIRED) {
        sim7600_mqtt_client_release(&s_mqtt_client);
    }
    if (saved_state >= MQTT_STATE_STARTED) {
        sim7600_mqtt_stop(&s_mqtt_client);
    }

    memset(&s_mqtt_client, 0, sizeof(s_mqtt_client));
    return ESP_OK;
}

esp_err_t SIM7600_mqtt_publish(const char* topic, const char* payload) {
    return sim7600_mqtt_publish(&s_mqtt_client, topic, payload, 1, false);
}
esp_err_t SIM7600_mqtt_publish_log(const char* topic, const char* payload) {
    return sim7600_mqtt_publish_log(&s_mqtt_client, topic, payload, 1, false);
}
esp_err_t SIM7600_mqtt_publish_data(const char* topic, const char* payload) {
    return sim7600_mqtt_publish_data(&s_mqtt_client, topic, payload, 1, false);
}
esp_err_t SIM7600_mqtt_subscribe(const char* topic) {
    return sim7600_mqtt_subscribe(&s_mqtt_client, topic, 1);
}

esp_err_t SIM7600_mqtt_unsubscribe(const char* topic) {
    return sim7600_mqtt_unsubscribe(&s_mqtt_client, topic);
}






