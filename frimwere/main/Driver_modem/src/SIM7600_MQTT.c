#include "SIM7600_MQTT.h"
#include "SIMCOM_Driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "LOG_Save_SD_Manager.h"



#define TAG_SIM7600_MQTT "SIM7600_MQTT"

/* Maximum retry counts */
#define MAX_RETRIES 3
#define PROMPT_TIMEOUT_MS 1000
#define COMMAND_TIMEOUT_MS 5000
#define CONNECT_TIMEOUT_MS 15000
#define MAX_RESPONSE_LINES 10
bool subscribe_status = false;
/* Global state */
static sim7600_mqtt_client_t g_mqtt_clients[2] = {0};
static sim7600_mqtt_message_callback_t g_msg_callback = NULL;
static sim7600_mqtt_event_callback_t g_event_callback = NULL;

/* Forward declarations */
static esp_err_t wait_for_prompt(uint32_t timeout_ms);
static esp_err_t wait_for_ok(uint32_t timeout_ms);
static esp_err_t wait_for_urc(const char* expected, uint32_t timeout_ms, char** response);
static esp_err_t send_command_with_data(const char* cmd, const char* data, size_t data_len, uint32_t timeout_ms);
static void update_client_state(sim7600_mqtt_client_t* client, sim7600_mqtt_state_t new_state, esp_err_t error);
static bool is_client_valid(sim7600_mqtt_client_t* client);

/*---------------------------------------------------------------------------*/
/* Initialization and Deinitialization                                       */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_init(void) {
    memset(&g_mqtt_clients, 0, sizeof(g_mqtt_clients));
    g_msg_callback = NULL;
    g_event_callback = NULL;
    
    printf("[%s] MQTT subsystem initialized\n", TAG_SIM7600_MQTT);
    return ESP_OK;
}

esp_err_t sim7600_mqtt_deinit(void) {
    /* Disconnect any active clients */
   // for (int i = 0; i < 2; i++) {
       // if (g_mqtt_clients[i].state >= MQTT_STATE_CONNECTED) {
            sim7600_mqtt_disconnect_broker(&g_mqtt_clients[0], 60);
       // }
      //  if (g_mqtt_clients[i].state >= MQTT_STATE_CLIENT_ACQUIRED) {
            sim7600_mqtt_client_release(&g_mqtt_clients[0]);
       // }
      //  if (g_mqtt_clients[i].state >= MQTT_STATE_STARTED) {
            sim7600_mqtt_stop(&g_mqtt_clients[0]);
            
       // }
    //}
    
    memset(&g_mqtt_clients, 0, sizeof(g_mqtt_clients));
    printf("[%s] MQTT subsystem deinitialized\n", TAG_SIM7600_MQTT);
    return ESP_OK; 
}

/*---------------------------------------------------------------------------*/
/* Client Management                                                         */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_client_acquire(sim7600_mqtt_client_t* client, const char* client_id, bool ssl_enabled) {
    if (!client || !client_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(client_id) == 0 || strlen(client_id) > sizeof(((sim7600_mqtt_client_t*)0)->client_id) - 1) {
        printf("[%s] Client ID too long (max %d bytes)\n", TAG_SIM7600_MQTT,
               (int)(sizeof(((sim7600_mqtt_client_t*)0)->client_id) - 1));
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret = ESP_FAIL;

    /* Find available client index if not provided */
    if (client->client_index != 0 && client->client_index != 1) {
        if (g_mqtt_clients[0].state == MQTT_STATE_IDLE) {
            client->client_index = 0;
        } else if (g_mqtt_clients[1].state == MQTT_STATE_IDLE) {
            client->client_index = 1;
        } else {
            printf("[%s] No available MQTT clients\n", TAG_SIM7600_MQTT);
            return ESP_FAIL;
        }
    }
    
    /* Send AT+CMQTTACCQ command */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=%d,\"%s\"%s\r\n", 
             client->client_index, client_id, ssl_enabled ? ",1" : "");
    
    printf("[%s] Acquiring client %d: %s\n", TAG_SIM7600_MQTT, client->client_index, client_id);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    ret = wait_for_ok(COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to acquire client\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Initialize client structure */
    strncpy(client->client_id, client_id, sizeof(client->client_id) - 1);
    client->client_id[sizeof(client->client_id) - 1] = '\0';
    client->ssl_enabled = ssl_enabled;
    client->state = MQTT_STATE_CLIENT_ACQUIRED;
    client->last_activity = xTaskGetTickCount();
    
    /* Save to global array */
    memcpy(&g_mqtt_clients[client->client_index], client, sizeof(sim7600_mqtt_client_t));
    
    update_client_state(client, MQTT_STATE_CLIENT_ACQUIRED, ESP_OK);
    
    printf("[%s] Client %d acquired successfully\n", TAG_SIM7600_MQTT, client->client_index);
    return ESP_OK;
}
esp_err_t sim7600_mqtt_disconnect_broker(sim7600_mqtt_client_t* client, uint16_t timeout_sec) {
    if (!is_client_valid(client)) {
    // return ESP_ERR_INVALID_ARG;
    }
    
    if (timeout_sec < 60 || timeout_sec > 180) {
        timeout_sec = 120;  /* Default */
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTDISC=%d,%d\r\n", client->client_index, timeout_sec);
    
    printf("[%s] Disconnecting client %d\n", TAG_SIM7600_MQTT, client->client_index);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for OK */
    esp_err_t ret = wait_for_ok(2000);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTDISC URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTDISC:", 10000, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTDISC: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            printf("[%s] Disconnect failed with error %d\n", TAG_SIM7600_MQTT, err_code);
            return ESP_FAIL;
        }
        
        client->state = MQTT_STATE_DISCONNECTED;
        g_mqtt_clients[client->client_index].state = MQTT_STATE_DISCONNECTED;
        update_client_state(client, MQTT_STATE_DISCONNECTED, ESP_OK);
        
        printf("[%s] Disconnected\n", TAG_SIM7600_MQTT);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t sim7600_mqtt_client_release(sim7600_mqtt_client_t* client) {
    if (!is_client_valid(client)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state >= MQTT_STATE_CONNECTED) {
        printf("[%s] Client %d is still connected, disconnect first\n", 
               TAG_SIM7600_MQTT, client->client_index);
        return ESP_FAIL;
    }
    
    char cmd[32];
    
    snprintf(cmd, sizeof(cmd), "AT+CMQTTREL=%d\r\n", client->client_index);
    
    printf("[%s] Releasing client %d\n", TAG_SIM7600_MQTT, client->client_index);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    if (wait_for_ok(COMMAND_TIMEOUT_MS) != ESP_OK) {
        printf("[%s] Failed to release client\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    snprintf(cmd, sizeof(cmd), "AT+CMQTTREL=0\r\n");
    flush_at_response_queue();
    UART_sendd(cmd);
    
    if (wait_for_ok(COMMAND_TIMEOUT_MS) != ESP_OK) {
        printf("[%s] Failed to release client\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    
    /* Save index before clearing the struct */
    uint8_t saved_index = client->client_index;

    /* Clear client structure */
    memset(client, 0, sizeof(sim7600_mqtt_client_t));

    /* Clear global entry */
    memset(&g_mqtt_clients[saved_index], 0, sizeof(sim7600_mqtt_client_t));
    
    printf("[%s] Client %d released\n", TAG_SIM7600_MQTT, client->client_index);
    return ESP_OK;
}
esp_err_t sim7600_mqtt_stop(sim7600_mqtt_client_t* client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    printf("[%s] Stopping MQTT service\n", TAG_SIM7600_MQTT);
    
    flush_at_response_queue();
    UART_sendd("AT+CMQTTSTOP\r\n");
    
    /* Wait for OK and +CMQTTSTOP: 0 */
    esp_err_t ret = wait_for_ok(2000);
    if (ret != ESP_OK) {
        return ret;
    }
    
    char* urc = NULL;
    ret = wait_for_urc("+CMQTTSTOP:", 5000, &urc);
    
    if (ret == ESP_OK && urc) {
        int err_code = 0;
        sscanf(urc, "+CMQTTSTOP: %d", &err_code);
        freeReceivedMessage(urc);
        
        if (err_code != 0) {
            return ESP_FAIL;
        }
        
        client->state = MQTT_STATE_IDLE;
        g_mqtt_clients[client->client_index].state = MQTT_STATE_IDLE;
        update_client_state(client, MQTT_STATE_IDLE, ESP_OK);
        
        printf("[%s] MQTT service stopped\n", TAG_SIM7600_MQTT);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}
/*---------------------------------------------------------------------------*/
/* Core MQTT Commands                                                        */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_start(sim7600_mqtt_client_t* client) {
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }

    printf("[%s] Starting MQTT service for client %d\n",
           TAG_SIM7600_MQTT, client->client_index);

    flush_at_response_queue();
    UART_sendd("AT+CMQTTSTART\r\n");

    /* The modem may send OK then +CMQTTSTART:N, or +CMQTTSTART:N then ERROR
     * (e.g. error 23 = already started). Collect all lines for up to 12 s. */
    int  urc_err_code = -1;   /* -1 = URC not yet seen */
    bool got_final    = false; /* OK or ERROR received */

    TickType_t start_tick   = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(12000);

    while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
        char* resp = UART_receive(200);
        if (resp) {
            printf("[%s] < %s\n", TAG_SIM7600_MQTT, resp);

            if (strstr(resp, "+CMQTTSTART:")) {
                sscanf(resp, "+CMQTTSTART: %d", &urc_err_code);
            }
            if (strstr(resp, "OK"))    { got_final = true; }
            if (strstr(resp, "ERROR")) { got_final = true; }

            freeReceivedMessage(resp);
        }

        /* Stop as soon as we have both the URC code and a final response */
        if (urc_err_code >= 0 && got_final) break;
    }

    /* Error code 0 = started OK; 23 = already running — both are success */
    if (urc_err_code == 0 || urc_err_code == 23) {
        client->state = MQTT_STATE_STARTED;
        g_mqtt_clients[client->client_index].state = MQTT_STATE_STARTED;
        update_client_state(client, MQTT_STATE_STARTED, ESP_OK);
        printf("[%s] MQTT service started (code=%d)\n", TAG_SIM7600_MQTT, urc_err_code);
        return ESP_OK;
    }

    if (urc_err_code > 0) {
        printf("[%s] MQTT start failed with error %d\n", TAG_SIM7600_MQTT, urc_err_code);
        return ESP_FAIL;
    }

    printf("[%s] MQTT start timeout (no URC received)\n", TAG_SIM7600_MQTT);
    return ESP_ERR_TIMEOUT;
}


esp_err_t sim7600_mqtt_set_utf8_check(sim7600_mqtt_client_t* client, bool enable) {
    if (!is_client_valid(client)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTCFG=\"checkUTF8\",%d,%d\r\n", 
             client->client_index, enable ? 1 : 0);
    
    printf("[%s] Setting UTF8 check to %d\n", TAG_SIM7600_MQTT, enable ? 1 : 0);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    return wait_for_ok(COMMAND_TIMEOUT_MS);
}

esp_err_t sim7600_mqtt_set_ssl_context(sim7600_mqtt_client_t* client, uint8_t ssl_ctx_index) {
    if (!is_client_valid(client) || !client->ssl_enabled) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ssl_ctx_index > 9) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSSLCFG=%d,%d\r\n", 
             client->client_index, ssl_ctx_index);
    
    printf("[%s] Setting SSL context %d for client %d\n", 
           TAG_SIM7600_MQTT, ssl_ctx_index, client->client_index);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    esp_err_t ret = wait_for_ok(COMMAND_TIMEOUT_MS);
    if (ret == ESP_OK) {
        client->ssl_ctx_index = ssl_ctx_index;
        g_mqtt_clients[client->client_index].ssl_ctx_index = ssl_ctx_index;
    }
    
    return ret;
}

/*---------------------------------------------------------------------------*/
/* Will Message Configuration                                                */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_set_will(sim7600_mqtt_client_t* client, const char* topic, const char* message, uint8_t qos) {
    if (!is_client_valid(client) || !topic || !message) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int topic_len = strlen(topic);
    int msg_len = strlen(message);
    
    if (topic_len < 1 || topic_len > 1024 || msg_len < 1 || msg_len > 10240) {
        printf("[%s] Will topic or message length out of range\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret;
    
    /* Set will topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLTOPIC=%d,%d\r\n", client->client_index, topic_len);
    
    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set will topic\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Set will message */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTWILLMSG=%d,%d,%d\r\n", client->client_index, msg_len, qos);
    
    ret = send_command_with_data(cmd, message, msg_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set will message\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    printf("[%s] Will message configured\n", TAG_SIM7600_MQTT);
    return ESP_OK;
}

/*---------------------------------------------------------------------------*/
/* Connection Functions                                                      */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Connection Functions                                                      */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_connect(sim7600_mqtt_client_t* client, const char* host, uint16_t port, 
                               uint16_t keepalive, uint8_t clean_session,
                               const char* username, const char* password) {
    if (!is_client_valid(client) || !host) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (keepalive < 60 || keepalive > 64800) {
        printf("[%s] Keepalive must be 60-64800 seconds\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Validate host length (max 256 total for full URL) */
    size_t host_len = strlen(host);
    if (host_len < 1 || host_len > 240) {  /* Leave room for "tcp://" and port */
        printf("[%s] Host name too long\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    char broker_url[300];  /* Large enough for full URL */
    char cmd[512];         /* Large enough for full command */
    char* resp = NULL;
    esp_err_t ret;
    
    /* Build broker URL in required format: "tcp://host:port" */
    if (client->ssl_enabled) {
        snprintf(broker_url, sizeof(broker_url), "ssl://%s:%d", host, port);
    } else {
        snprintf(broker_url, sizeof(broker_url), "tcp://%s:%d", host, port);
    }
    
    /* Ensure service is started */
    if (client->state < MQTT_STATE_STARTED) {
        ret = sim7600_mqtt_start(client);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    /* Build connect command */
    if (username && password && strlen(username) > 0 && strlen(password) > 0) {
        snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=%d,\"%s\",%d,%d,\"%s\",\"%s\"\r\n",
                 client->client_index, broker_url, keepalive, clean_session, username, password);
    } else if (username && strlen(username) > 0) {
        snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=%d,\"%s\",%d,%d,\"%s\"\r\n",
                 client->client_index, broker_url, keepalive, clean_session, username);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CMQTTCONNECT=%d,\"%s\",%d,%d\r\n",
                 client->client_index, broker_url, keepalive, clean_session);
    }
    
    printf("[%s] Connecting to %s\n", TAG_SIM7600_MQTT, broker_url);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for OK */
    ret = wait_for_ok(CONNECT_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] No OK response to connect\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Wait for +CMQTTCONNECT URC */
    ret = wait_for_urc("+CMQTTCONNECT:", CONNECT_TIMEOUT_MS, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        
        /* Parse response: +CMQTTCONNECT: <client_index>,<err> */
        if (sscanf(resp, "+CMQTTCONNECT: %d,%d", &idx, &err_code) == 2) {
            freeReceivedMessage(resp);
            
            if (err_code != 0) {
                printf("[%s] Connection failed with error %d\n", TAG_SIM7600_MQTT, err_code);
                update_client_state(client, client->state, ESP_FAIL);
                
                /* Map error codes to meaningful messages */
                switch(err_code) {
                    case 1:  printf("[%s] Reason: Failed (general failure)\n", TAG_SIM7600_MQTT); break;
                    case 2:  printf("[%s] Reason: Bad UTF-8 string\n", TAG_SIM7600_MQTT); break;
                    case 3:  printf("[%s] Reason: Socket connect fail\n", TAG_SIM7600_MQTT); break;
                    case 4:  printf("[%s] Reason: Socket create fail\n", TAG_SIM7600_MQTT); break;
                    case 5:  printf("[%s] Reason: Socket close fail\n", TAG_SIM7600_MQTT); break;
                    case 9:  printf("[%s] Reason: Network not opened\n", TAG_SIM7600_MQTT); break;
                    case 16: printf("[%s] Reason: Socket sending fail\n", TAG_SIM7600_MQTT); break;
                    case 17: printf("[%s] Reason: Timeout\n", TAG_SIM7600_MQTT); break;
                    case 25: printf("[%s] Reason: DNS error\n", TAG_SIM7600_MQTT); break;
                    case 26: printf("[%s] Reason: Socket closed by server\n", TAG_SIM7600_MQTT); break;
                    case 27: printf("[%s] Reason: Connection refused - protocol version\n", TAG_SIM7600_MQTT); break;
                    case 28: printf("[%s] Reason: Connection refused - identifier rejected\n", TAG_SIM7600_MQTT); break;
                    case 29: printf("[%s] Reason: Connection refused - server unavailable\n", TAG_SIM7600_MQTT); break;
                    case 30: printf("[%s] Reason: Connection refused - bad user/password\n", TAG_SIM7600_MQTT); break;
                    case 31: printf("[%s] Reason: Connection refused - not authorized\n", TAG_SIM7600_MQTT); break;
                    case 32: printf("[%s] Reason: Handshake fail\n", TAG_SIM7600_MQTT); break;
                    case 33: printf("[%s] Reason: Certificate not set\n", TAG_SIM7600_MQTT); break;
                }
                
                return ESP_FAIL;
            }
            
            /* Success - update client state */
            client->state = MQTT_STATE_CONNECTED;
            strncpy(client->broker_url, broker_url, sizeof(client->broker_url) - 1);
            client->broker_url[sizeof(client->broker_url) - 1] = '\0';
            client->keepalive = keepalive;
            client->clean_session = clean_session;
            g_mqtt_clients[client->client_index].state = MQTT_STATE_CONNECTED;
            
            update_client_state(client, MQTT_STATE_CONNECTED, ESP_OK);
            
            printf("[%s] Connected to broker successfully\n", TAG_SIM7600_MQTT);
            return ESP_OK;
        } else {
            freeReceivedMessage(resp);
        }
    }
    
    return ESP_FAIL;
}



/* Optional: Convenience function for default port (1883) */
esp_err_t sim7600_mqtt_connect_default(sim7600_mqtt_client_t* client, const char* host,
                                       uint16_t keepalive, uint8_t clean_session) {
    return sim7600_mqtt_connect(client, host, 1883, keepalive, clean_session, NULL, NULL);
}

/* Optional: SSL version with default port (8883) */
esp_err_t sim7600_mqtt_connect_ssl(sim7600_mqtt_client_t* client, const char* host,
                                   uint16_t keepalive, uint8_t clean_session,
                                   const char* username, const char* password) {
    if (!client->ssl_enabled) {
        printf("[%s] Client not configured for SSL\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    return sim7600_mqtt_connect(client, host, 8883, keepalive, clean_session, username, password);
}



/*---------------------------------------------------------------------------*/
/* Publish Functions                                                         */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_publish(sim7600_mqtt_client_t* client, const char* topic, 
                               const char* payload, uint8_t qos, bool retained) {
    if (!is_client_valid(client) || !topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        printf("[%s] Not connected to broker\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    
    size_t topic_len = strlen(topic);
    size_t payload_len = strlen(payload);
    
    if (topic_len < 1 || topic_len > 1024 || payload_len < 1 || payload_len > 10240) {
        printf("[%s] Topic or payload length out of range\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret;
    
    printf("[%s] Publishing to topic: %s (QoS %d)\n", TAG_SIM7600_MQTT, topic, qos);
    
    /* Set topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=%d,%d\r\n", client->client_index, (int)topic_len);
    
    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish topic\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Set payload */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=%d,%d\r\n", client->client_index, (int)payload_len);
    
    ret = send_command_with_data(cmd, payload, payload_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish payload\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Publish */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=%d,%d,%d,%d\r\n", 
             client->client_index, qos, 60, retained ? 1 : 0);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for OK */
    ret = wait_for_ok(5000);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTPUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTPUB:", 10000, &resp);
    
 /*   if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            printf("[%s] Publish failed with error %d\n", TAG_SIM7600_MQTT, err_code);
            // here add save the playload in to SD card on file .json name by tyhe date and hour min s




            return ESP_FAIL;
        }
        
        printf("[%s] Published successfully\n", TAG_SIM7600_MQTT);
        return ESP_OK;
    }*/
    if (ret == ESP_OK && resp) {
    int err_code = 0;
    int idx = 0;
    sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
    freeReceivedMessage(resp);
    
    if (err_code != 0) {
          //  save_failed_payload_to_sd(topic, payload, err_code);
        
        return ESP_FAIL;
    }
    }
    return ESP_FAIL;
}


esp_err_t sim7600_mqtt_publish_data(sim7600_mqtt_client_t* client, const char* topic, 
                               const char* payload, uint8_t qos, bool retained) {
    if (!is_client_valid(client) || !topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        printf("[%s] Not connected to broker\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    
    size_t topic_len = strlen(topic);
    size_t payload_len = strlen(payload);
    
    if (topic_len < 1 || topic_len > 1024 || payload_len < 1 || payload_len > 10240) {
        printf("[%s] Topic or payload length out of range\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret;
    
    printf("[%s] Publishing to topic: %s (QoS %d)\n", TAG_SIM7600_MQTT, topic, qos);

    /* Set topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=%d,%d\r\n", client->client_index, (int)topic_len);

    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish topic — saving payload to SD\n", TAG_SIM7600_MQTT);
        sd_save_failed_payload(payload, ret);
        return ret;
    }

    /* Set payload */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=%d,%d\r\n", client->client_index, (int)payload_len);

    ret = send_command_with_data(cmd, payload, payload_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish payload — saving payload to SD\n", TAG_SIM7600_MQTT);
        sd_save_failed_payload(payload, ret);
        return ret;
    }

    /* Publish */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=%d,%d,%d,%d\r\n",
             client->client_index, qos, 60, retained ? 1 : 0);

    flush_at_response_queue();
    UART_sendd(cmd);

    /* Wait for OK */
    ret = wait_for_ok(5000);
    if (ret != ESP_OK) {
        printf("[%s] No OK for CMQTTPUB — saving payload to SD\n", TAG_SIM7600_MQTT);
        sd_save_failed_payload(payload, ret);
        return ret;
    }

    /* Wait for +CMQTTPUB URC */
    char *resp = NULL;
    ret = wait_for_urc("+CMQTTPUB:", 10000, &resp);

    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);

        if (err_code != 0) {
            printf("[%s] Publish failed (err %d) — saving payload to SD\n",
                   TAG_SIM7600_MQTT, err_code);
            sd_save_failed_payload(payload, err_code);
            return ESP_FAIL;
        }

        printf("[%s] Published successfully\n", TAG_SIM7600_MQTT);
        return ESP_OK;
    }

    /* URC timed out — treat as failure and save payload */
    printf("[%s] No publish confirmation — saving payload to SD\n", TAG_SIM7600_MQTT);
    sd_save_failed_payload(payload, ESP_FAIL);
    return ESP_FAIL;
}


esp_err_t sim7600_mqtt_publish_log(sim7600_mqtt_client_t* client, const char* topic, 
                               const char* payload, uint8_t qos, bool retained) {
    if (!is_client_valid(client) || !topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        printf("[%s] Not connected to broker\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    
    size_t topic_len = strlen(topic);
    size_t payload_len = strlen(payload);
    
    if (topic_len < 1 || topic_len > 1024 || payload_len < 1 || payload_len > 10240) {
        printf("[%s] Topic or payload length out of range\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret;
    
    printf("[%s] Publishing to topic: %s (QoS %d)\n", TAG_SIM7600_MQTT, topic, qos);
    
    /* Set topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=%d,%d\r\n", client->client_index, (int)topic_len);
    
    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish topic\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Set payload */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=%d,%d\r\n", client->client_index, (int)payload_len);
    
    ret = send_command_with_data(cmd, payload, payload_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set publish payload\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Publish */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=%d,%d,%d,%d\r\n", 
             client->client_index, qos, 60, retained ? 1 : 0);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for OK */
    ret = wait_for_ok(5000);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTPUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTPUB:", 10000, &resp);
    
 /*   if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            printf("[%s] Publish failed with error %d\n", TAG_SIM7600_MQTT, err_code);
            // here add save the playload in to SD card on file .json name by tyhe date and hour min s




            return ESP_FAIL;
        }
        
        printf("[%s] Published successfully\n", TAG_SIM7600_MQTT);
        return ESP_OK;
    }*/
    if (ret == ESP_OK && resp) {
    int err_code = 0;
    int idx = 0;
    sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
    freeReceivedMessage(resp);
    
    if (err_code != 0) {
          //  save_failed_payload_to_sd(topic, payload, err_code);
        
        return ESP_FAIL;
    }
    }
    return ESP_FAIL;
}

esp_err_t sim7600_mqtt_publish_hex(sim7600_mqtt_client_t* client, const char* topic,
                                   const uint8_t* data, size_t len, uint8_t qos, bool retained) {
    if (!is_client_valid(client) || !topic || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2 || len > 10240) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        return ESP_FAIL;
    }
    
    size_t topic_len = strlen(topic);
    char cmd[64];
    esp_err_t ret;
    
    /* Set topic */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=%d,%d\r\n", client->client_index, (int)topic_len);
    
    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Set payload length */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=%d,%d\r\n", client->client_index, (int)len);
    
    printf("[%s] AT> %s", TAG_SIM7600_MQTT, cmd);
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for prompt */
    ret = wait_for_prompt(PROMPT_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Send binary data */
    printf("[%s] Sending %d bytes of binary data\n", TAG_SIM7600_MQTT, (int)len);
    
    /* For binary data, send raw bytes directly via UART driver */
    for (size_t i = 0; i < len; i++) {
        uart_write_bytes(UART_NUM, (const char*)&data[i], 1);
    }
    
    /* Wait for OK */
    ret = wait_for_ok(COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Publish */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=%d,%d,%d,%d\r\n", 
             client->client_index, qos, 60, retained ? 1 : 0);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    ret = wait_for_ok(5000);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTPUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTPUB:", 10000, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTPUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            return ESP_FAIL;
        }
        
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

/*---------------------------------------------------------------------------*/
/* Subscribe Functions                                                       */
/*---------------------------------------------------------------------------*/

esp_err_t sim7600_mqtt_subscribe(sim7600_mqtt_client_t* client, const char* topic, uint8_t qos) {
    if (!is_client_valid(client) || !topic) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (qos > 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        return ESP_FAIL;
    }
    
    int topic_len = strlen(topic);
    if (topic_len < 1 || topic_len > 1024) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char cmd[64];
    esp_err_t ret;
    
    printf("[%s] Subscribing to: %s (QoS %d)\n", TAG_SIM7600_MQTT, topic, qos);
    
    /* Single topic subscribe */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=%d,%d,%d\r\n", 
             client->client_index, topic_len, qos);
    
    ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("[%s] Failed to set subscribe topic\n", TAG_SIM7600_MQTT);
        return ret;
    }
    
    /* Wait for +CMQTTSUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTSUB:", 10000, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTSUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            printf("[%s] Subscribe failed with error %d\n", TAG_SIM7600_MQTT, err_code);
            return ESP_FAIL;
        }
        
        client->state = MQTT_STATE_SUBSCRIBED;
        g_mqtt_clients[client->client_index].state = MQTT_STATE_SUBSCRIBED;
        update_client_state(client, MQTT_STATE_SUBSCRIBED, ESP_OK);
        
        printf("[%s] Subscribed successfully\n", TAG_SIM7600_MQTT);
        subscribe_status=true;
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t sim7600_mqtt_subscribe_multiple(sim7600_mqtt_client_t* client, 
                                          const char* topics[], uint8_t* qos, size_t count) {
    if (!is_client_valid(client) || !topics || !qos || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (count > 10) {
        printf("[%s] Too many topics for one subscribe\n", TAG_SIM7600_MQTT);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        return ESP_FAIL;
    }
    
    /* Calculate total length */
    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        total_len += strlen(topics[i]) + 1;  /* +1 for separator */
    }
    
    char* combined = malloc(total_len + 1);
    if (!combined) {
        return ESP_ERR_NO_MEM;
    }
    
    combined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(combined, ",");
        strcat(combined, topics[i]);
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTSUB=%d,%d\r\n", 
             client->client_index, (int)strlen(combined));
    
    esp_err_t ret = send_command_with_data(cmd, combined, strlen(combined), COMMAND_TIMEOUT_MS);
    free(combined);
    
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTSUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTSUB:", 10000, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTSUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            return ESP_FAIL;
        }
        
        client->state = MQTT_STATE_SUBSCRIBED;
        g_mqtt_clients[client->client_index].state = MQTT_STATE_SUBSCRIBED;
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t sim7600_mqtt_unsubscribe(sim7600_mqtt_client_t* client, const char* topic) {
    if (!is_client_valid(client) || !topic) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (client->state < MQTT_STATE_CONNECTED) {
        return ESP_FAIL;
    }
    
    int topic_len = strlen(topic);
    char cmd[64];
    
    snprintf(cmd, sizeof(cmd), "AT+CMQTTUNSUB=%d,%d,0\r\n", client->client_index, topic_len);
    
    esp_err_t ret = send_command_with_data(cmd, topic, topic_len, COMMAND_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    /* Wait for +CMQTTUNSUB URC */
    char* resp = NULL;
    ret = wait_for_urc("+CMQTTUNSUB:", 10000, &resp);
    
    if (ret == ESP_OK && resp) {
        int err_code = 0;
        int idx = 0;
        sscanf(resp, "+CMQTTUNSUB: %d,%d", &idx, &err_code);
        freeReceivedMessage(resp);
        
        if (err_code != 0) {
            return ESP_FAIL;
        }
        
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

/*---------------------------------------------------------------------------*/
/* URC Processing                                                            */
/*---------------------------------------------------------------------------*/

void sim7600_mqtt_process_urc(const char* line) {
    if (!line) return;
    
    /* Check for +CMQTTRXSTART - beginning of received message */
    if (strstr(line, "+CMQTTRXSTART:")) {
        int client_idx, topic_len, payload_len;
        if (sscanf(line, "+CMQTTRXSTART: %d,%d,%d", &client_idx, &topic_len, &payload_len) == 3) {
            printf("[%s] Receiving message for client %d: topic len=%d, payload len=%d\n", 
                   TAG_SIM7600_MQTT, client_idx, topic_len, payload_len);
        }
    }
    
    /* Check for +CMQTTRXTOPIC - topic data */
    else if (strstr(line, "+CMQTTRXTOPIC:")) {
        int client_idx, topic_len;
        if (sscanf(line, "+CMQTTRXTOPIC: %d,%d", &client_idx, &topic_len) == 2) {
            /* The actual topic will be in the next line(s) */
        }
    }
    
    /* Check for +CMQTTRXPAYLOAD - payload data */
    else if (strstr(line, "+CMQTTRXPAYLOAD:")) {
        int client_idx, payload_len;
        if (sscanf(line, "+CMQTTRXPAYLOAD: %d,%d", &client_idx, &payload_len) == 2) {
            /* The actual payload will be in the next line(s) */
        }
    }
    
    /* Check for +CMQTTRXEND - end of message */
    else if (strstr(line, "+CMQTTRXEND:")) {
        int client_idx;
        if (sscanf(line, "+CMQTTRXEND: %d", &client_idx) == 1) {
            printf("[%s] Message reception complete for client %d\n", TAG_SIM7600_MQTT, client_idx);
        }
    }
    
    /* Check for +CMQTTCONNOST - passive disconnection */
    else if (strstr(line, "+CMQTTCONNOST:")) {
        int client_idx, cause;
        if (sscanf(line, "+CMQTTCONNOST: %d,%d", &client_idx, &cause) == 2) {
            printf("[%s] Client %d disconnected by server, cause=%d\n", 
                   TAG_SIM7600_MQTT, client_idx, cause);
            
            if (client_idx >= 0 && client_idx < 2) {
                g_mqtt_clients[client_idx].state = MQTT_STATE_DISCONNECTED;
                update_client_state(&g_mqtt_clients[client_idx], MQTT_STATE_DISCONNECTED, ESP_FAIL);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Callback Registration                                                     */
/*---------------------------------------------------------------------------*/

void sim7600_mqtt_register_message_callback(sim7600_mqtt_message_callback_t callback) {
    g_msg_callback = callback;
    printf("[%s] Message callback registered\n", TAG_SIM7600_MQTT);
}

void sim7600_mqtt_register_event_callback(sim7600_mqtt_event_callback_t callback) {
    g_event_callback = callback;
    printf("[%s] Event callback registered\n", TAG_SIM7600_MQTT);
}

/*---------------------------------------------------------------------------*/
/* Utility Functions                                                         */
/*---------------------------------------------------------------------------*/

const char* sim7600_mqtt_state_to_str(sim7600_mqtt_state_t state) {
    switch (state) {
        case MQTT_STATE_IDLE: return "IDLE";
        case MQTT_STATE_STARTED: return "STARTED";
        case MQTT_STATE_CLIENT_ACQUIRED: return "CLIENT_ACQUIRED";
        case MQTT_STATE_CONNECTED: return "CONNECTED";
        case MQTT_STATE_SUBSCRIBED: return "SUBSCRIBED";
        case MQTT_STATE_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

const char* sim7600_mqtt_error_to_str(int err_code) {
    switch (err_code) {
        case 0:  return "Success";
        case 1:  return "Failed (general failure)";
        case 2:  return "Bad UTF-8 string";
        case 3:  return "Socket connect fail";
        case 4:  return "Socket create fail";
        case 5:  return "Socket close fail";
        case 9:  return "Network not opened";
        case 16: return "Socket sending fail";
        case 17: return "Timeout";
        case 25: return "DNS error";
        case 26: return "Socket closed by server";
        case 27: return "Connection refused - protocol version";
        case 28: return "Connection refused - identifier rejected";
        case 29: return "Connection refused - server unavailable";
        case 30: return "Connection refused - bad user/password";
        case 31: return "Connection refused - not authorized";
        case 32: return "Handshake fail";
        case 33: return "Certificate not set";
        default: return "Unknown error";
    }
}

/*---------------------------------------------------------------------------*/
/* Internal Helper Functions                                                 */
/*---------------------------------------------------------------------------*/

static bool is_client_valid(sim7600_mqtt_client_t* client) {
    return (client && (client->client_index == 0 || client->client_index == 1) &&
            client->state != MQTT_STATE_IDLE);
}

static void update_client_state(sim7600_mqtt_client_t* client, sim7600_mqtt_state_t new_state, esp_err_t error) {
    client->state = new_state;
    client->last_activity = xTaskGetTickCount();
    
    if (g_event_callback) {
        g_event_callback(client->client_index, new_state, error);
    }
}

static esp_err_t wait_for_prompt(uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        char* resp = UART_receive(200);
        if (resp) {
            printf("[%s] < %s\n", TAG_SIM7600_MQTT, resp);
            
            if (strchr(resp, '>')) {
                freeReceivedMessage(resp);
                return ESP_OK;
            }
            
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            
            freeReceivedMessage(resp);
        }
    }
    
    return ESP_ERR_TIMEOUT;
}

static esp_err_t wait_for_ok(uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        char* resp = UART_receive(200);
        if (resp) {
            printf("[%s] < %s\n", TAG_SIM7600_MQTT, resp);
            
            if (strstr(resp, "OK")) {
                freeReceivedMessage(resp);
                return ESP_OK;
            }
            
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            
            freeReceivedMessage(resp);
        }
    }
    
    return ESP_ERR_TIMEOUT;
}

static esp_err_t wait_for_urc(const char* expected, uint32_t timeout_ms, char** response) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    *response = NULL;
    
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        char* resp = UART_receive(200);
        if (resp) {
            printf("[%s] < %s\n", TAG_SIM7600_MQTT, resp);
            
            if (strstr(resp, expected)) {
                *response = resp;  /* Caller must free */
                return ESP_OK;
            }
            
            if (strstr(resp, "ERROR") || strstr(resp, "error")) {
                freeReceivedMessage(resp);
                return ESP_FAIL;
            }
            
            freeReceivedMessage(resp);
        }
    }
    
    return ESP_ERR_TIMEOUT;
}

static esp_err_t send_command_with_data(const char* cmd, const char* data, size_t data_len, uint32_t timeout_ms) {
    printf("[%s] AT> %s", TAG_SIM7600_MQTT, cmd);
    
    flush_at_response_queue();
    UART_sendd(cmd);
    
    /* Wait for prompt */
    if (wait_for_prompt(PROMPT_TIMEOUT_MS) != ESP_OK) {
        printf("[%s] No prompt received\n", TAG_SIM7600_MQTT);
        return ESP_FAIL;
    }
    
    /* Send data */
    printf("[%s] > %.*s\n", TAG_SIM7600_MQTT, (int)data_len, data);
    flush_at_response_queue();
    
    /* Send data in one go */
    UART_sendd(data);
    
    /* Wait for OK */
    return wait_for_ok(timeout_ms);
}