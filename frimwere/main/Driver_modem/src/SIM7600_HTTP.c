/**
 * @file SIM7600_HTTP.c
 * @brief Implementation of SIMCOM SIM7600 driver for HTTP/HTTPS operations
 * 
 * This file contains the implementation of HTTP/HTTPS GET/POST functions
 * with SSL/TLS and certificate authentication for the SIM7600 module.
 * 
 * @author Your Name
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "SIM7600_HTTP.h"
#include "SIMCOM_Driver.h"
#include "cJSON.h"

/**
 * @brief Tag for SIM7600 HTTP logging
 */
#define TAG_SIM7600_HTTP "SIM7600_HTTP"

/**
 * @brief Last SSL error message
 */
static char last_ssl_error[256] = {0};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * @brief Extract JSON response from AT command response
 */
static char* extract_json_response(char *response) {
    if (!response) return NULL;
    
    char *json_start = strchr(response, '{');
    if (!json_start) return NULL;
    
    return strdup(json_start);
}

/**
 * @brief Extract data from response using delimiters
 */
static char* extract_data(const char *response, const char *start_delim, const char *end_delim) {
    if (!response || !start_delim || !end_delim) return NULL;
    
    char *start = strstr(response, start_delim);
    if (!start) return NULL;
    
    start += strlen(start_delim);
    char *end = strstr(start, end_delim);
    if (!end) return NULL;
    
    size_t len = end - start;
    char *result = (char*)malloc(len + 1);
    if (result) {
        strncpy(result, start, len);
        result[len] = '\0';
    }
    
    return result;
}

/**
 * @brief Check if response contains OK
 */
static bool response_is_ok(char *response) {
    return (response && strstr(response, "OK") != NULL);
}

/**
 * @brief Check if response contains ERROR
 */
static bool response_is_error(char *response) {
    return (response && strstr(response, "ERROR") != NULL);
}

/**
 * @brief Process server response (placeholder function)
 */
static void process_server_response(char *response, int len) {
    if (!response || len <= 0) return;

    cJSON *root = cJSON_ParseWithLength(response, len);
    if (!root) {
        /* Not JSON — log raw */
        ESP_LOGI(TAG_SIM7600_HTTP, "Server response (raw): %s", response);
        return;
    }

    /* ── Sync API response: {"accepted":N,"duplicates":N,"rejected":N} ─── */
    cJSON *accepted   = cJSON_GetObjectItem(root, "accepted");
    cJSON *duplicates = cJSON_GetObjectItem(root, "duplicates");
    cJSON *rejected   = cJSON_GetObjectItem(root, "rejected");

    if (cJSON_IsNumber(accepted) && cJSON_IsNumber(duplicates) && cJSON_IsNumber(rejected)) {
        int acc = accepted->valueint;
        int dup = duplicates->valueint;
        int rej = rejected->valueint;

        ESP_LOGI(TAG_SIM7600_HTTP, "╔══════════════════════════════════╗");
        ESP_LOGI(TAG_SIM7600_HTTP, "║       SERVER SYNC RESULT         ║");
        ESP_LOGI(TAG_SIM7600_HTTP, "╠══════════════════════════════════╣");
        ESP_LOGI(TAG_SIM7600_HTTP, "║  ✓ accepted   : %-4d             ║", acc);
        ESP_LOGI(TAG_SIM7600_HTTP, "║  ~ duplicates : %-4d             ║", dup);
        ESP_LOGI(TAG_SIM7600_HTTP, "║  ✗ rejected   : %-4d             ║", rej);
        ESP_LOGI(TAG_SIM7600_HTTP, "╚══════════════════════════════════╝");

        if (rej > 0) {
            ESP_LOGW(TAG_SIM7600_HTTP, "%d reading(s) rejected by server!", rej);
        }
        if (acc == 0 && dup == 0 && rej == 0) {
            ESP_LOGW(TAG_SIM7600_HTTP, "Server accepted nothing — check payload format");
        }
    } else {
        /* Other JSON response (config update, OTA, etc.) — log as-is */
        char *pretty = cJSON_PrintUnformatted(root);
        if (pretty) {
            ESP_LOGI(TAG_SIM7600_HTTP, "Server response: %s", pretty);
            free(pretty);
        }
    }

    cJSON_Delete(root);
}

/**
 * @brief Process server response for measurement (placeholder function)
 */
static void process_server_response_masure(char *response, int len) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Measurement response: %s", response);
}

/**
 * @brief Process server response for update (placeholder function)
 */
static esp_err_t process_server_response_masure_update(char *response, int len) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Update response: %s", response);
    return ESP_OK;
}

/*============================================================================
 * Basic HTTP Functions
 *============================================================================*/

esp_err_t SIMcom_http_get_SIM7600(char *server, int port, char *url) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTP GET - %s:%d%s", server, port, url);
    
    char *received_message = NULL;
    char *get_len = NULL;
    char *json_response = NULL;
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        ESP_LOGD(TAG_SIM7600_HTTP, "HTTPINIT: %s", received_message);
        freeReceivedMessage(received_message);
    }
    
    // Set URL parameter
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"http://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Execute HTTP GET action
    UART_sendd("AT+HTTPACTION=0\r\n");
    received_message = UART_receive(5000);
    if (received_message) {
        if (strstr(received_message, "+HTTPACTION:") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "HTTP GET completed");
        }
        freeReceivedMessage(received_message);
    }
    
    // Get HTTP headers
    UART_sendd("AT+HTTPHEAD\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Get response length
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(1000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }
    
    // Read response data
    if (get_len) {
        ESP_LOGI(TAG_SIM7600_HTTP, "Response length: %s", get_len);
        
        char atCommand[SIM7600_CMD_MAX_LEN];
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%s\r\n", get_len);
        
        UART_sendd(atCommand);
        received_message = UART_receive(5000);
        if (received_message) {
            json_response = extract_json_response(received_message);
            if (json_response) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Response: %s", json_response);
                process_server_response(json_response, strlen(json_response));
                free(json_response);
            }
            freeReceivedMessage(received_message);
        }
        free(get_len);
    }
    
    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    return ESP_OK;
}

esp_err_t SIMcom_http_post_SIM7600(char *server, int port, char *url, char *json_str) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTP POST - %s:%d%s", server, port, url);
    
    char *received_message = NULL;
    char *get_len = NULL;
    
    if (!json_str) {
        ESP_LOGE(TAG_SIM7600_HTTP, "JSON string is NULL");
        return ESP_FAIL;
    }
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set URL parameter
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"http://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Prepare and send POST data
    size_t request_len = strlen(json_str);
    char atCommand[SIM7600_CMD_MAX_LEN];
    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,30000\r\n", request_len);
    
    UART_sendd(atCommand);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Send actual data
    UART_sendd(json_str);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Wait for data acceptance
    int i = 0;
    do {
        received_message = UART_receive(1000);
        if (received_message) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Data response: %s", received_message);
            freeReceivedMessage(received_message);
        }
        i++;
    } while (i <= (request_len / 500) + 2);
    
    // Sync
    UART_sendd("AT\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Execute HTTP POST action
    UART_sendd("AT+HTTPACTION=1\r\n");
    received_message = UART_receive(5000);
    if (received_message) {
        if (strstr(received_message, "+HTTPACTION:") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "HTTP POST completed");
        }
        freeReceivedMessage(received_message);
    }
    
    // Get HTTP headers
    UART_sendd("AT+HTTPHEAD\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Get response length
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(1000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }
    
    // Read response data
    if (get_len) {
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%s\r\n", get_len);
        UART_sendd(atCommand);
        received_message = UART_receive(5000);
        
        if (received_message) {
            char *response1 = extract_data(received_message, "{", "\n");
            if (response1) {
                char *modified = calloc(strlen(received_message) + 2, 1);
                if (modified) {
                    snprintf(modified, strlen(received_message) + 2, "{%s", response1);
                    process_server_response_masure(modified, strlen(modified));
                    free(modified);
                }
                free(response1);
            }
            freeReceivedMessage(received_message);
        }
        free(get_len);
    }
    
    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    return ESP_OK;
}

esp_err_t SIMcom_http_post_SIM7600_update(char *server, int port, char *url, char *json_str) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTP POST Update");
    
    char *received_message = NULL;
    char *get_len = NULL;
    esp_err_t err = ESP_FAIL;
    
    if (!json_str) {
        ESP_LOGE(TAG_SIM7600_HTTP, "JSON string is NULL");
        return ESP_FAIL;
    }
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set URL parameter
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"http://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Prepare and send POST data
    size_t request_len = strlen(json_str);
    char atCommand[SIM7600_CMD_MAX_LEN];
    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,30000\r\n", request_len);
    
    UART_sendd(atCommand);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Send actual data
    UART_sendd(json_str);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    int i = 0;
    do {
        received_message = UART_receive(1000);
        if (received_message) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Data: %s", received_message);
            freeReceivedMessage(received_message);
        }
        i++;
    } while (i <= (request_len / 500) + 2);
    
    // Execute HTTP POST action
    UART_sendd("AT+HTTPACTION=1\r\n");
    received_message = UART_receive(5000);
    if (received_message) {
        ESP_LOGD(TAG_SIM7600_HTTP, "HTTPACTION: %s", received_message);
        freeReceivedMessage(received_message);
    }
    
    // Get HTTP headers
    UART_sendd("AT+HTTPHEAD\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Get response length
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(1000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }
    
    // Read response data
    if (get_len) {
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%s\r\n", get_len);
        UART_sendd(atCommand);
        received_message = UART_receive(5000);
        
        if (received_message) {
            int len = strlen(received_message);
            char *response1 = extract_data(received_message, "{", "\n");
            
            if (response1) {
                char *get = (char *)malloc(len + 2);
                if (get) {
                    snprintf(get, len + 2, "{%s", response1);
                    ESP_LOGI(TAG_SIM7600_HTTP, "Request %s", get);
                    
                    if (len < 18) {
                        err = process_server_response_masure_update(get, len);
                    }
                    
                    free(get);
                }
                free(response1);
            }
            freeReceivedMessage(received_message);
        }
        free(get_len);
    }
    
    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    return err;
}

/*============================================================================
 * Certificate Management Functions
 *============================================================================*/

esp_err_t SIMcom_list_certificates(void) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Listing certificates");
    
    UART_sendd("AT+CCERTLIST\r\n");
    char *response = UART_receive(3000);
    
    if (response) {
        if (strstr(response, "+CCERTLIST:") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "Certificates: %s", response);
        } else if (strstr(response, "OK") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "No certificates found");
        }
        freeReceivedMessage(response);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

esp_err_t SIMcom_delete_certificate(const char *filename) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Deleting certificate: %s", filename);
    
    char command[SIM7600_CMD_MAX_LEN];
    snprintf(command, sizeof(command), "AT+CCERTDELE=\"%s\"\r\n", filename);
    
    UART_sendd(command);
    char *response = UART_receive(3000);
    
    if (response) {
        if (strstr(response, "OK") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "Certificate deleted");
            freeReceivedMessage(response);
            return ESP_OK;
        } else if (strstr(response, "ERROR") != NULL) {
            ESP_LOGW(TAG_SIM7600_HTTP, "Certificate not found or cannot be deleted");
        }
        freeReceivedMessage(response);
    }
    
    return ESP_FAIL;
}

esp_err_t SIMcom_download_certificate(const char *filename, const char *cert_content) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Downloading certificate: %s", filename);
    
    size_t cert_len = strlen(cert_content);
    char command[SIM7600_CMD_MAX_LEN];
    char *response = NULL;
    esp_err_t ret = ESP_FAIL;
    
    // First, make sure module is responsive
    UART_sendd("AT\r\n");
    char *test_response = UART_receive(1000);
    if (test_response) {
        ESP_LOGD(TAG_SIM7600_HTTP, "Module responsive: %s", test_response);
        freeReceivedMessage(test_response);
    } else {
        ESP_LOGE(TAG_SIM7600_HTTP, "Module not responding");
        return ESP_FAIL;
    }
    
    // Clear UART buffer
    test_response = UART_receive(100);
    if (test_response) freeReceivedMessage(test_response);
    
    // Send download command
    snprintf(command, sizeof(command), "AT+CCERTDOWN=\"%s\",%d\r\n", filename, cert_len);
    UART_sendd(command);
    
    // Wait for '>' prompt
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Read until we see '>'
    int prompt_found = 0;
    int timeout = 0;
    
    while (!prompt_found && timeout < 10) {
        response = UART_receive(1000);
        if (response) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Prompt response: %s", response);
            if (strchr(response, '>') != NULL) {
                prompt_found = 1;
            }
            freeReceivedMessage(response);
        }
        timeout++;
    }
    
    if (!prompt_found) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to get download prompt");
        return ESP_FAIL;
    }
    
    // Send certificate content
    ESP_LOGD(TAG_SIM7600_HTTP, "Sending certificate (%d bytes)...", cert_len);
    uart_write_bytes(UART_NUM, cert_content, cert_len);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Send newline
    UART_sendd("\r\n");
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Read all responses until we find OK
    int ok_received = 0;
    int error_received = 0;
    timeout = 0;
    
    while (!ok_received && !error_received && timeout < 30) {
        response = UART_receive(1000);
        if (response) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Response: %s", response);
            
            if (strstr(response, "OK") != NULL) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Certificate downloaded successfully");
                ok_received = 1;
                ret = ESP_OK;
            } else if (strstr(response, "ERROR") != NULL) {
                ESP_LOGE(TAG_SIM7600_HTTP, "Certificate download failed: %s", response);
                error_received = 1;
            }
            
            freeReceivedMessage(response);
        } else {
            timeout++;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Final check - try to list certificates to verify
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        UART_sendd("AT+CCERTLIST\r\n");
        response = UART_receive(3000);
        if (response) {
            if (strstr(response, filename) != NULL) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Certificate verified in module");
            }
            freeReceivedMessage(response);
        }
    }
    
    return ret;
}

esp_err_t SIMcom_download_certificate_from_file(const char *filename, const char *filepath) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Loading certificate from file: %s", filepath);
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to open certificate file: %s", filepath);
        return ESP_FAIL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > SIM7600_CERT_MAX_LEN) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Invalid certificate file size: %ld", file_size);
        fclose(f);
        return ESP_FAIL;
    }
    
    // Read certificate content
    char *cert_content = (char *)malloc(file_size + 1);
    if (!cert_content) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for certificate");
        fclose(f);
        return ESP_FAIL;
    }
    
    size_t read_size = fread(cert_content, 1, file_size, f);
    cert_content[read_size] = '\0';
    fclose(f);
    
    if (read_size != file_size) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to read certificate file");
        free(cert_content);
        return ESP_FAIL;
    }
    
    // Download to module
    esp_err_t ret = SIMcom_download_certificate(filename, cert_content);
    free(cert_content);
    
    return ret;
}

/*============================================================================
 * SSL/TLS Configuration Functions
 *============================================================================*/

esp_err_t SIMcom_set_ssl_version(ssl_version_t ssl_version) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Setting SSL version to %d", ssl_version);
    
    char command[SIM7600_CMD_MAX_LEN];
    snprintf(command, sizeof(command), "AT+CSSLCFG=\"sslversion\",0,%d\r\n", ssl_version);
    
    UART_sendd(command);
    char *response = UART_receive(1000);
    
    if (response && strstr(response, "OK") != NULL) {
        freeReceivedMessage(response);
        return ESP_OK;
    }
    
    if (response) freeReceivedMessage(response);
    return ESP_FAIL;
}

esp_err_t SIMcom_set_sni(bool enable) {
    ESP_LOGI(TAG_SIM7600_HTTP, "%s SNI", enable ? "Enabling" : "Disabling");
    
    char command[SIM7600_CMD_MAX_LEN];
    snprintf(command, sizeof(command), "AT+CSSLCFG=\"enableSNI\",0,%d\r\n", enable ? 1 : 0);
    
    UART_sendd(command);
    char *response = UART_receive(1000);
    
    if (response && strstr(response, "OK") != NULL) {
        freeReceivedMessage(response);
        return ESP_OK;
    }
    
    if (response) freeReceivedMessage(response);
    return ESP_FAIL;
}

esp_err_t SIMcom_ignore_ssl_validation(bool ignore) {
    ESP_LOGW(TAG_SIM7600_HTTP, "%s SSL validation", ignore ? "Ignoring" : "Enabling");
    
    char command[SIM7600_CMD_MAX_LEN];
    snprintf(command, sizeof(command), "AT+CSSLCFG=\"ignorelocaltime\",0,%d\r\n", ignore ? 1 : 0);
    
    UART_sendd(command);
    char *response = UART_receive(1000);
    
    if (response && strstr(response, "OK") != NULL) {
        freeReceivedMessage(response);
        return ESP_OK;
    }
    
    if (response) freeReceivedMessage(response);
    return ESP_FAIL;
}

esp_err_t SIMcom_clear_ssl_config(void) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Clearing SSL configuration");
    
    UART_sendd("AT+CSSLCFG=\"clearconf\",0\r\n");
    char *response = UART_receive(1000);
    
    if (response && strstr(response, "OK") != NULL) {
        freeReceivedMessage(response);
        return ESP_OK;
    }
    
    if (response) freeReceivedMessage(response);
    return ESP_FAIL;
}

esp_err_t SIMcom_configure_ssl(const char *ca_cert_filename, 
                               const char *client_cert_filename,
                               const char *client_key_filename,
                               ssl_auth_mode_t auth_mode) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Configuring SSL (auth mode: %d)", auth_mode);
    
    char command[SIM7600_CMD_MAX_LEN];
    char *response = NULL;
    
    // Set SSL version to TLS 1.2
    SIMcom_set_ssl_version(SSL_VERSION_ALL);
    
    // Set authentication mode
    snprintf(command, sizeof(command), "AT+CSSLCFG=\"authmode\",0,%d\r\n", auth_mode);
    UART_sendd(command);
    response = UART_receive(1000);
    if (response) freeReceivedMessage(response);
    
    // Configure CA certificate
    if (ca_cert_filename) {
        snprintf(command, sizeof(command), "AT+CSSLCFG=\"cacert\",0,\"%s\"\r\n", ca_cert_filename);
        UART_sendd(command);
        response = UART_receive(1000);
        if (response) {
            if (strstr(response, "OK") != NULL) {
                ESP_LOGI(TAG_SIM7600_HTTP, "CA certificate configured");
            } else {
                ESP_LOGE(TAG_SIM7600_HTTP, "Failed to configure CA certificate");
                freeReceivedMessage(response);
                return ESP_FAIL;
            }
            freeReceivedMessage(response);
        }
    }
    
    // Configure client certificate
    if (client_cert_filename) {
        snprintf(command, sizeof(command), "AT+CSSLCFG=\"clientcert\",0,\"%s\"\r\n", client_cert_filename);
        UART_sendd(command);
        response = UART_receive(1000);
        if (response) {
            if (strstr(response, "OK") != NULL) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Client certificate configured");
            } else {
                ESP_LOGE(TAG_SIM7600_HTTP, "Failed to configure client certificate");
                freeReceivedMessage(response);
                return ESP_FAIL;
            }
            freeReceivedMessage(response);
        }
    }
    
    // Configure client key
    if (client_key_filename) {
        snprintf(command, sizeof(command), "AT+CSSLCFG=\"clientkey\",0,\"%s\"\r\n", client_key_filename);
        UART_sendd(command);
        response = UART_receive(1000);
        if (response) {
            if (strstr(response, "OK") != NULL) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Client key configured");
            } else {
                ESP_LOGE(TAG_SIM7600_HTTP, "Failed to configure client key");
                freeReceivedMessage(response);
                return ESP_FAIL;
            }
            freeReceivedMessage(response);
        }
    }
    
    // Enable SNI
    SIMcom_set_sni(true);
    
    ESP_LOGI(TAG_SIM7600_HTTP, "SSL configuration completed");
    return ESP_OK;
}

/*============================================================================
 * HTTPS Functions
 *============================================================================*/

esp_err_t SIMcom_https_get_SIM7600(char *server, int port, char *url, bool use_auth) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS GET - %s:%d%s", server, port, url);
    
    char *received_message = NULL;
    char *get_len = NULL;
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Enable SSL for HTTPS
    UART_sendd("AT+HTTPSSL=1\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set URL with https://
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        UART_sendd("AT+HTTPTERM\r\n");
        UART_receive(1000);
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"https://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set timeout (longer for HTTPS)
    UART_sendd("AT+HTTPPARA=\"TIMEOUT\",30000\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Execute HTTP GET action
    UART_sendd("AT+HTTPACTION=0\r\n");
    received_message = UART_receive(10000);
    if (received_message) {
        if (strstr(received_message, "+HTTPACTION: 0,200") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS GET successful (200 OK)");
        } else if (strstr(received_message, "+HTTPACTION:") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS GET response: %s", received_message);
        } else if (strstr(received_message, "+HTTP_PEER_CLOSED") != NULL) {
            ESP_LOGE(TAG_SIM7600_HTTP, "SSL handshake failed - check certificates");
            snprintf(last_ssl_error, sizeof(last_ssl_error), 
                     "SSL handshake failed - certificate issue");
        }
        freeReceivedMessage(received_message);
    }
    
    // Get response length
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(1000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }
    
    // Read response data
    if (get_len) {
        char atCommand[SIM7600_CMD_MAX_LEN];
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%s\r\n", get_len);
        
        UART_sendd(atCommand);
        received_message = UART_receive(10000);
        if (received_message) {
            char *json_response = extract_json_response(received_message);
            if (json_response) {
                ESP_LOGI(TAG_SIM7600_HTTP, "Response: %s", json_response);
                process_server_response(json_response, strlen(json_response));
                free(json_response);
            }
            freeReceivedMessage(received_message);
        }
        free(get_len);
    }
    
    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    return ESP_OK;
}

esp_err_t SIMcom_https_post_SIM7600(char *server, int port, char *url, char *json_str, bool use_auth) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS POST - %s:%d%s", server, port, url);
    
    char *received_message = NULL;
    char *get_len = NULL;
    
    if (!json_str) {
        ESP_LOGE(TAG_SIM7600_HTTP, "JSON string is NULL");
        return ESP_FAIL;
    }
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Enable SSL for HTTPS
    UART_sendd("AT+HTTPSSL=1\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set URL with https://
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        UART_sendd("AT+HTTPTERM\r\n");
        UART_receive(1000);
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"https://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set timeout for HTTPS
    UART_sendd("AT+HTTPPARA=\"TIMEOUT\",30000\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Prepare and send POST data
    size_t request_len = strlen(json_str);
    char atCommand[SIM7600_CMD_MAX_LEN];
    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,30000\r\n", request_len);
    
    UART_sendd(atCommand);
    received_message = UART_receive(5000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Send actual data
    UART_sendd(json_str);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    int i = 0;
    do {
        received_message = UART_receive(1000);
        if (received_message) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Data response: %s", received_message);
            freeReceivedMessage(received_message);
        }
        i++;
    } while (i <= (request_len / 500) + 2);
    
    // Execute HTTP POST action and parse status code
    int http_status = 0;
    bool post_success = false;

    UART_sendd("AT+HTTPACTION=1\r\n");
    received_message = UART_receive(10000);
    if (received_message) {
        /* Parse  +HTTPACTION: 1,<status_code>,<data_len>  */
        char *action = strstr(received_message, "+HTTPACTION: 1,");
        if (action) {
            sscanf(action, "+HTTPACTION: 1,%d", &http_status);
        }

        if (http_status >= 200 && http_status < 300) {
            post_success = true;
            ESP_LOGI(TAG_SIM7600_HTTP,
                     "┌─────────────────────────────────────────┐");
            ESP_LOGI(TAG_SIM7600_HTTP,
                     "│  HTTPS POST ✓  HTTP %d                  │", http_status);
            ESP_LOGI(TAG_SIM7600_HTTP,
                     "└─────────────────────────────────────────┘");
        } else if (http_status > 0) {
            ESP_LOGE(TAG_SIM7600_HTTP,
                     "┌─────────────────────────────────────────┐");
            ESP_LOGE(TAG_SIM7600_HTTP,
                     "│  HTTPS POST ✗  HTTP %d                  │", http_status);
            ESP_LOGE(TAG_SIM7600_HTTP,
                     "└─────────────────────────────────────────┘");
        } else if (strstr(received_message, "+HTTP_PEER_CLOSED") != NULL) {
            ESP_LOGE(TAG_SIM7600_HTTP,
                     "HTTPS POST ✗  SSL handshake failed — check server certificate");
            snprintf(last_ssl_error, sizeof(last_ssl_error),
                     "SSL handshake failed");
        } else {
            ESP_LOGE(TAG_SIM7600_HTTP,
                     "HTTPS POST ✗  No valid HTTPACTION response: %s",
                     received_message);
        }
        freeReceivedMessage(received_message);
    } else {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTPS POST ✗  Modem timeout (no HTTPACTION response)");
    }

    // Read and log the server response body
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }

    if (get_len) {
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%s\r\n", get_len);
        UART_sendd(atCommand);
        received_message = UART_receive(10000);

        if (received_message) {
            char *json_response = extract_json_response(received_message);
            if (json_response) {
                /* process_server_response handles all parsing and logging */
                process_server_response(json_response, strlen(json_response));
                free(json_response);
            } else {
                ESP_LOGI(TAG_SIM7600_HTTP,
                         "Server response (raw): %s", received_message);
            }
            freeReceivedMessage(received_message);
        }
        free(get_len);
    }

    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    return post_success ? ESP_OK : ESP_FAIL;
}

/* ── Multipart/form-data boundary ─────────────────────────────────────────── */
#define MULTIPART_BOUNDARY "ESP32Boundary"

esp_err_t SIMcom_https_post_multipart_SIM7600(char *server, int port, char *url,
                                               const char *json_data, bool use_auth)
{
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS POST multipart - https://%s%s", server, url);

    if (!json_data) {
        ESP_LOGE(TAG_SIM7600_HTTP, "json_data is NULL");
        return ESP_FAIL;
    }

    char *received_message = NULL;
    esp_err_t result       = ESP_FAIL;

    /* ── Build multipart/form-data body ──────────────────────────────────────
     * Structure:
     *   --boundary\r\n
     *   Content-Disposition: form-data; name="file"; filename="readings.json"\r\n
     *   Content-Type: application/json\r\n
     *   \r\n
     *   <json_data>\r\n
     *   --boundary--\r\n
     * ──────────────────────────────────────────────────────────────────────── */
    size_t json_len  = strlen(json_data);
    size_t body_alloc = json_len + 300;   /* 300 bytes covers all multipart overhead */

    char *body = malloc(body_alloc);
    if (!body) {
        ESP_LOGE(TAG_SIM7600_HTTP, "OOM building multipart body");
        return ESP_ERR_NO_MEM;
    }

    int written = snprintf(body, body_alloc,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"readings.json\"\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "%s\r\n"
        "--%s--\r\n",
        MULTIPART_BOUNDARY, json_data, MULTIPART_BOUNDARY);

    if (written <= 0 || (size_t)written >= body_alloc) {
        ESP_LOGE(TAG_SIM7600_HTTP, "multipart body build error (written=%d)", written);
        free(body);
        return ESP_FAIL;
    }
    size_t body_len = (size_t)written;
    ESP_LOGI(TAG_SIM7600_HTTP, "multipart body ready: %d bytes (json=%d)", (int)body_len, (int)json_len);

    /* ════════════════════════════════════════════════════════════════════════
     * AT command sequence — per official SIMCom HTTP AT Command Manual V1.00.
     *
     * Key corrections from previous version:
     *   SSLCFG (not SSLCTXID) — documented param to bind SSL context
     *   CONTENT               — documented param for Content-Type (max 256 B)
     *   https:// prefix       — activates SSL; AT+HTTPSSL not in the manual
     *   CONNECTTO / RECVTO    — documented timeout params (not "TIMEOUT")
     *   CID / USERDATA        — removed (not in the official HTTP manual)
     * ════════════════════════════════════════════════════════════════════════ */

    /* ── 1. Close any stale HTTP session ──────────────────────────────────── */
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    /* ── 2. Configure SSL context 0 (SSL AT Command Manual) ──────────────────
     * authmode=0       skip server-cert verification (no CA cert on device)
     * sslversion=4     accept TLS 1.0 / 1.1 / 1.2 / 1.3
     * ignorelocaltime  ignore cert expiry (RTC may not be NTP-synced yet)
     * enableSNI=1      send correct hostname in TLS ClientHello              */
    UART_sendd("AT+CSSLCFG=\"authmode\",0,0\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    UART_sendd("AT+CSSLCFG=\"sslversion\",0,4\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    UART_sendd("AT+CSSLCFG=\"ignorelocaltime\",0,1\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    UART_sendd("AT+CSSLCFG=\"enableSNI\",0,1\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    /* ── 3. Start HTTP service ─────────────────────────────────────────────── */
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(3000);
    if (received_message) {
        if (response_is_error(received_message))
            ESP_LOGE(TAG_SIM7600_HTTP, "HTTPINIT failed: %s", received_message);
        freeReceivedMessage(received_message);
    }

    /* ── 4. Bind to SSL context 0 — documented as SSLCFG (not SSLCTXID) ───── */
    UART_sendd("AT+HTTPPARA=\"SSLCFG\",\"0\"\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    /* ── 4b. Explicitly enable SSL — AT+HTTPSSL=1 is not in the official manual
     *  but this firmware requires it.  Evidence: without it the modem returns
     *  no HTTPACTION URC at all (local abort in < 200 ms); with it, the modem
     *  successfully completes the TLS handshake and reaches the server (400). */
    UART_sendd("AT+HTTPSSL=1\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        if (!response_is_ok(received_message))
            ESP_LOGW(TAG_SIM7600_HTTP, "HTTPSSL response: %s", received_message);
        freeReceivedMessage(received_message);
    }

    /* ── 5. URL — https:// prefix triggers SSL; no port for standard 443 ───── */
    char *urll = malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        free(body);
        UART_sendd("AT+HTTPTERM\r\n");
        received_message = UART_receive(1000);
        if (received_message) freeReceivedMessage(received_message);
        return ESP_FAIL;
    }
    snprintf(urll, SIM7600_URL_MAX_LEN,
             "AT+HTTPPARA=\"URL\",\"https://%s%s\"\r\n", server, url);
    ESP_LOGI(TAG_SIM7600_HTTP, "URL → https://%s%s", server, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);

    /* ── 6. Content-Type — documented CONTENT param, max 256 chars ─────────── */
    char content_cmd[128];
    snprintf(content_cmd, sizeof(content_cmd),
             "AT+HTTPPARA=\"CONTENT\",\"multipart/form-data; boundary=%s\"\r\n",
             MULTIPART_BOUNDARY);
    UART_sendd(content_cmd);
    received_message = UART_receive(1000);
    if (received_message) {
        if (!response_is_ok(received_message))
            ESP_LOGW(TAG_SIM7600_HTTP, "CONTENT param: %s", received_message);
        freeReceivedMessage(received_message);
    }

    /* ── 7. Timeouts — CONNECTTO (seconds) and RECVTO (seconds) ────────────── */
    UART_sendd("AT+HTTPPARA=\"CONNECTTO\",120\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    UART_sendd("AT+HTTPPARA=\"RECVTO\",20\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    /* ── Feed data to modem via HTTPDATA ─────────────────────────────────── */

    /* Flush any stale AT responses that accumulated during setup so the
     * DOWNLOAD prompt and HTTPDATA OK come from a clean queue.           */
    flush_at_response_queue();

    char atCommand[SIM7600_CMD_MAX_LEN];
    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,30000\r\n", (int)body_len);
    UART_sendd(atCommand);

    /* Wait for DOWNLOAD prompt — modem signals it is ready to receive bytes.
     * Use a generous 15 s window in case the modem is slow to respond.   */
    bool got_download = false;
    for (int t = 0; t < 15 && !got_download; t++) {
        received_message = UART_receive(1000);
        if (received_message) {
            if (strstr(received_message, "DOWNLOAD")) got_download = true;
            freeReceivedMessage(received_message);
        }
    }
    if (!got_download) {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTPDATA: no DOWNLOAD prompt — aborting");
        free(body);
        UART_sendd("AT+HTTPTERM\r\n");
        received_message = UART_receive(1000);
        if (received_message) freeReceivedMessage(received_message);
        return ESP_FAIL;
    }

    /* Send the multipart body bytes.
     * uart_write_bytes copies to the driver TX FIFO and returns immediately;
     * the hardware then clocks bytes out at 115200 baud asynchronously.
     * At 115200 baud, 7 KB takes ~620 ms.  Wait until physically on the wire
     * before polling for the modem's OK so no stale queue item can mask it. */
    uart_write_bytes(UART_NUM, body, body_len);
    free(body);

    /* Wait for TX to drain — every byte physically sent before we listen */
    uart_wait_tx_done(UART_NUM, pdMS_TO_TICKS(5000));

    /* Small guard gap — modem needs a moment to process the body          */
    vTaskDelay(pdMS_TO_TICKS(300));

    /* Wait for OK — use wall-clock deadline so stale queue items that return
     * UART_receive fast don't exhaust the attempt count before OK arrives. */
    bool got_ok = false;
    TickType_t ok_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(15000); /* 15 s max */
    while (!got_ok && xTaskGetTickCount() < ok_deadline) {
        received_message = UART_receive(1000);
        if (received_message) {
            if (strstr(received_message, "OK"))    { got_ok = true; }
            if (strstr(received_message, "ERROR")) {
                ESP_LOGE(TAG_SIM7600_HTTP, "HTTPDATA ERROR: modem rejected the body data");
                freeReceivedMessage(received_message);
                UART_sendd("AT+HTTPTERM\r\n");
                received_message = UART_receive(1000);
                if (received_message) freeReceivedMessage(received_message);
                return ESP_FAIL;
            }
            freeReceivedMessage(received_message);
        }
    }
    if (!got_ok) {
        ESP_LOGW(TAG_SIM7600_HTTP, "HTTPDATA: no OK after data send — continuing anyway");
    }

    /* ── Execute POST (HTTPACTION=1) ─────────────────────────────────────── */
    int http_status = 0;
    UART_sendd("AT+HTTPACTION=1\r\n");

    /* HTTPS on onrender can take up to 50 s (cold start); poll 5 s × 12 = 60 s max */
    for (int t = 0; t < 12 && http_status == 0; t++) {
        received_message = UART_receive(5000);
        if (received_message) {
            char *action = strstr(received_message, "+HTTPACTION: 1,");
            if (action) {
                sscanf(action, "+HTTPACTION: 1,%d", &http_status);
            }
            if (strstr(received_message, "+HTTP_PEER_CLOSED")) {
                ESP_LOGE(TAG_SIM7600_HTTP, "HTTPS POST ✗  SSL peer closed connection");
                freeReceivedMessage(received_message);
                break;
            }
            freeReceivedMessage(received_message);
        }
    }

    if (http_status >= 200 && http_status < 300) {
        ESP_LOGI(TAG_SIM7600_HTTP,
                 "┌─────────────────────────────────────────┐");
        ESP_LOGI(TAG_SIM7600_HTTP,
                 "│  HTTPS POST ✓  HTTP %d                  │", http_status);
        ESP_LOGI(TAG_SIM7600_HTTP,
                 "└─────────────────────────────────────────┘");
        result = ESP_OK;
    } else if (http_status > 0) {
        ESP_LOGE(TAG_SIM7600_HTTP,
                 "┌─────────────────────────────────────────┐");
        ESP_LOGE(TAG_SIM7600_HTTP,
                 "│  HTTPS POST ✗  HTTP %d                  │", http_status);
        ESP_LOGE(TAG_SIM7600_HTTP,
                 "└─────────────────────────────────────────┘");
    } else {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTPS POST ✗  No HTTPACTION response (timeout or SSL error)");
    }

    /* ── Read server response body ───────────────────────────────────────────
     * AT+HTTPREAD? replies: "+HTTPREAD: LEN,<n>"
     * We must extract after "LEN," not after "+HTTPREAD: ".               */
    int resp_len = 0;
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        char *len_ptr = strstr(received_message, "LEN,");
        if (len_ptr) {
            resp_len = atoi(len_ptr + 4);   /* skip "LEN," */
        }
        freeReceivedMessage(received_message);
    }

    if (resp_len > 0) {
        snprintf(atCommand, sizeof(atCommand), "AT+HTTPREAD=0,%d\r\n", resp_len);
        UART_sendd(atCommand);

        /* The modem sends the HTTPREAD response in up to 3 UART frames:
         *   Frame A : "+HTTPREAD: 0,<n>\r\n<body>\r\nOK\r\n"  (all-in-one)
         *   Frame B : "+HTTPREAD: 0,<n>\r\n"  then  "<body>\r\n"  then  "OK\r\n"
         *
         * A stale "OK" from a previous AT command can also sit in the RX buffer.
         * Strategy: loop on !resp_body — ignore any frame that has no body,
         * pick up the body whenever it arrives (up to 10 × 2 s).               */
        char *resp_body = NULL;

        for (int t = 0; t < 10 && !resp_body; t++) {
            received_message = UART_receive(2000);
            if (!received_message) continue;

            /* ── Case A: frame contains the JSON object directly ─────────────── */
            char *json_start = strchr(received_message, '{');
            if (json_start) {
                resp_body = strdup(json_start);   /* may include trailing \r\nOK */
            }

            /* ── Case B: frame has the +HTTPREAD: header; body follows the '\n' ─ */
            if (!resp_body) {
                char *hdr = strstr(received_message, "+HTTPREAD:");
                if (hdr) {
                    char *nl = strchr(hdr, '\n');
                    if (nl) {
                        nl++;   /* advance past the '\n' of the header line */
                        /* clip at the trailing \r\nOK if present */
                        char *ok_ptr = strstr(nl, "\r\nOK");
                        if (!ok_ptr) ok_ptr = strstr(nl, "\nOK");
                        size_t frag = ok_ptr ? (size_t)(ok_ptr - nl) : strlen(nl);
                        if (frag > 0) {
                            resp_body = (char *)malloc(frag + 1);
                            if (resp_body) {
                                memcpy(resp_body, nl, frag);
                                resp_body[frag] = '\0';
                            }
                        }
                    }
                }
            }

            /* Frame had no usable body (e.g. a stale "OK") — try next frame */
            freeReceivedMessage(received_message);
        }

        if (resp_body) {
            char *js = strchr(resp_body, '{');
            if (js) {
                process_server_response(js, strlen(js));
            } else {
                /* Non-JSON body — log raw, trimming \r\n */
                int rlen = strlen(resp_body);
                while (rlen > 0 && (resp_body[rlen-1] == '\r' || resp_body[rlen-1] == '\n'))
                    resp_body[--rlen] = '\0';
                ESP_LOGI(TAG_SIM7600_HTTP, "Server response (raw): %s", resp_body);
            }
            free(resp_body);
        } else {
            ESP_LOGW(TAG_SIM7600_HTTP,
                     "HTTPREAD: could not extract body (resp_len=%d)", resp_len);
        }
    }

    /* ── Terminate ───────────────────────────────────────────────────────── */
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);

    return result;
}

esp_err_t SIMcom_https_request_SIM7600(char *server, int port, char *url,
                                       http_method_t method, char *data, bool use_auth) {
    switch (method) {
        case HTTP_METHOD_GET:
            return SIMcom_https_get_SIM7600(server, port, url, use_auth);
        case HTTP_METHOD_POST:
            return SIMcom_https_post_SIM7600(server, port, url, data, use_auth);
        case HTTP_METHOD_PUT:
            // For PUT, we'd need to modify the method number
            ESP_LOGE(TAG_SIM7600_HTTP, "PUT method not implemented");
            return ESP_FAIL;
        case HTTP_METHOD_DELETE:
            // For DELETE, method number is 3
            ESP_LOGE(TAG_SIM7600_HTTP, "DELETE method not implemented");
            return ESP_FAIL;
        default:
            ESP_LOGE(TAG_SIM7600_HTTP, "Unsupported HTTP method");
            return ESP_FAIL;
    }
}

esp_err_t SIMcom_https_upload_file(char *server, int port, char *url,
                                   char *file_data, size_t data_len,
                                   char *content_type, bool use_auth) {
    ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS file upload - %s:%d%s", server, port, url);
    
    char *received_message = NULL;
    
    // Initialize HTTP service
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Enable SSL for HTTPS
    UART_sendd("AT+HTTPSSL=1\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set URL with https://
    char *urll = (char *)malloc(SIM7600_URL_MAX_LEN);
    if (!urll) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate memory for URL");
        UART_sendd("AT+HTTPTERM\r\n");
        UART_receive(1000);
        return ESP_FAIL;
    }
    
    snprintf(urll, SIM7600_URL_MAX_LEN, "AT+HTTPPARA=\"URL\",\"https://%s:%d%s\"\r\n", server, port, url);
    UART_sendd(urll);
    free(urll);
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set content type
    char content_cmd[SIM7600_CMD_MAX_LEN];
    snprintf(content_cmd, sizeof(content_cmd), "AT+HTTPPARA=\"CONTENT\",\"%s\"\r\n", content_type);
    UART_sendd(content_cmd);
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Set timeout
    UART_sendd("AT+HTTPPARA=\"TIMEOUT\",60000\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Send data
    char atCommand[SIM7600_CMD_MAX_LEN];
    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,60000\r\n", data_len);
    
    UART_sendd(atCommand);
    received_message = UART_receive(5000);
    if (received_message) freeReceivedMessage(received_message);
    
    // Send actual data
    UART_sendd(file_data);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Execute HTTP POST action
    UART_sendd("AT+HTTPACTION=1\r\n");
    received_message = UART_receive(30000);
    if (received_message) {
        if (strstr(received_message, "+HTTPACTION: 1,200") != NULL ||
            strstr(received_message, "+HTTPACTION: 1,201") != NULL) {
            ESP_LOGI(TAG_SIM7600_HTTP, "File upload successful");
        }
        freeReceivedMessage(received_message);
    }
    
    // Terminate HTTP service
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(1000);
    if (received_message) freeReceivedMessage(received_message);
    
    return ESP_OK;
}

/*============================================================================
 * Download File in Chunks with Progress Callback
 *============================================================================*/

// Global file pointer for callback
static FILE *downloaded_file = NULL;

/**
 * @brief Download file from GitHub in chunks with callback
 * @param github_raw_url Full raw GitHub URL
 * @param chunk_size Size of each chunk to process (e.g., 4096 bytes)
 * @param callback Function to call with each chunk
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_download_from_github_chunked(const char *github_raw_url, 
                                              size_t chunk_size,
                                              download_callback_t callback) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Chunked download from GitHub");
    
    char *received_message = NULL;
    char *get_len = NULL;
    esp_err_t ret = ESP_FAIL;
    size_t total_downloaded = 0;
    size_t file_size = 0;
    int redirect_count = 0;
    const int max_redirects = 3;
    
    // Start with HTTP URL
    char current_url[170];
    if (strncmp(github_raw_url, "https://", 8) == 0) {
        snprintf(current_url, sizeof(current_url), "http://%s", github_raw_url + 8);
        ESP_LOGI(TAG_SIM7600_HTTP, "Starting with HTTP URL: %s", current_url);
    } else {
        snprintf(current_url, sizeof(current_url), "%s", github_raw_url);
    }
    
download_attempt:
    ESP_LOGI(TAG_SIM7600_HTTP, "Download attempt %d, URL: %s", redirect_count + 1, current_url);
    
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Initialize HTTP
    UART_sendd("AT+HTTPINIT\r\n");
    received_message = UART_receive(5000);
    if (received_message) {
        ESP_LOGD(TAG_SIM7600_HTTP, "HTTPINIT: %s", received_message);
        if (strstr(received_message, "ERROR") != NULL) {
            ESP_LOGE(TAG_SIM7600_HTTP, "HTTPINIT failed");
            freeReceivedMessage(received_message);
            return ESP_FAIL;
        }
        freeReceivedMessage(received_message);
    }
    
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set URL
    char url_cmd[SIM7600_URL_MAX_LEN];
    int cmd_len = snprintf(url_cmd, sizeof(url_cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", current_url);
    if (cmd_len >= sizeof(url_cmd)) {
        ESP_LOGW(TAG_SIM7600_HTTP, "URL command truncated, trying alternative format");
        snprintf(url_cmd, sizeof(url_cmd), "AT+HTTPPARA=URL,%s\r\n", current_url);
    }
    
    ESP_LOGD(TAG_SIM7600_HTTP, "Setting URL");
    UART_sendd(url_cmd);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    received_message = UART_receive(5000);
    if (received_message) {
        ESP_LOGD(TAG_SIM7600_HTTP, "URL set response: %s", received_message);
        if (strstr(received_message, "ERROR") != NULL) {
            ESP_LOGE(TAG_SIM7600_HTTP, "Failed to set URL");
            freeReceivedMessage(received_message);
            goto cleanup;
        }
        freeReceivedMessage(received_message);
    }
    
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set timeout (some modules don't support this parameter)
    UART_sendd("AT+HTTPPARA=\"TIMEOUT\",30000\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        if (strstr(received_message, "ERROR") != NULL) {
            ESP_LOGW(TAG_SIM7600_HTTP, "Failed to set timeout, continuing anyway");
        }
        freeReceivedMessage(received_message);
    }
    
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set content type
    UART_sendd("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"\r\n");
    received_message = UART_receive(2000);
    if (received_message) {
        if (strstr(received_message, "ERROR") != NULL) {
            ESP_LOGW(TAG_SIM7600_HTTP, "Failed to set content type, continuing anyway");
        }
        freeReceivedMessage(received_message);
    }
    
    // Feed watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Execute GET
    ESP_LOGI(TAG_SIM7600_HTTP, "Executing HTTP GET...");
    UART_sendd("AT+HTTPACTION=0\r\n");
    
    // Wait for response
    vTaskDelay(pdMS_TO_TICKS(5000));
    received_message = UART_receive(10000);
    
    if (!received_message) {
        ESP_LOGE(TAG_SIM7600_HTTP, "No response to HTTPACTION");
        goto cleanup;
    }
    
    ESP_LOGD(TAG_SIM7600_HTTP, "HTTPACTION response: %s", received_message);
    
    // Check for redirect
    if (strstr(received_message, "+HTTPACTION: 0,301") != NULL ||
        strstr(received_message, "+HTTPACTION: 0,302") != NULL ||
        strstr(received_message, "+HTTPACTION: 0,307") != NULL) {
        
        ESP_LOGW(TAG_SIM7600_HTTP, "Received redirect (301/302/307)");
        freeReceivedMessage(received_message);
        
        // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Get headers - this will return the actual HTTP headers
        UART_sendd("AT+HTTPHEAD\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Read the header response
        char *header_response = UART_receive(5000);
        if (header_response) {
            ESP_LOGD(TAG_SIM7600_HTTP, "Header response: %s", header_response);
            
            // Now read the actual header data that follows
            vTaskDelay(pdMS_TO_TICKS(500));
            char *header_data = UART_receive(5000);
            
            if (header_data) {
                ESP_LOGD(TAG_SIM7600_HTTP, "Header data: %s", header_data);
                
                // Look for Location header in the actual HTTP headers
                char *location = strstr(header_data, "Location: ");
                if (!location) {
                    location = strstr(header_data, "location: ");
                }
                
                if (location) {
                    location = strstr(location, "://"); // Find the start of URL
                    if (location) {
                        char *end = strstr(location, "\r\n");
                        if (!end) end = strstr(location, "\n");
                        
                        if (end) {
                            size_t loc_len = end - location;
                            if (loc_len < sizeof(current_url)) {
                                strncpy(current_url, location, loc_len);
                                current_url[loc_len] = '\0';
                                ESP_LOGI(TAG_SIM7600_HTTP, "Redirecting to: %s", current_url);
                                
                                freeReceivedMessage(header_response);
                                freeReceivedMessage(header_data);
                                
                                // Terminate current session
                                UART_sendd("AT+HTTPTERM\r\n");
                                received_message = UART_receive(2000);
                                if (received_message) freeReceivedMessage(received_message);
                                
                                redirect_count++;
                                if (redirect_count < max_redirects) {
                                    vTaskDelay(pdMS_TO_TICKS(1000));
                                    goto download_attempt;
                                } else {
                                    ESP_LOGE(TAG_SIM7600_HTTP, "Too many redirects");
                                    goto cleanup;
                                }
                            }
                        }
                    }
                } else {
                    ESP_LOGD(TAG_SIM7600_HTTP, "No Location header found in header data");
                }
                freeReceivedMessage(header_data);
            } else {
                ESP_LOGD(TAG_SIM7600_HTTP, "No header data received");
            }
            freeReceivedMessage(header_response);
        } else {
            ESP_LOGD(TAG_SIM7600_HTTP, "No header response");
        }
        
        ESP_LOGE(TAG_SIM7600_HTTP, "Redirect received but could not extract location");
        goto cleanup;
    }
    
    // Check for success (200 OK)
    if (strstr(received_message, "+HTTPACTION: 0,200") == NULL) {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTP GET failed with code: %s", received_message);
        freeReceivedMessage(received_message);
        goto cleanup;
    }
    freeReceivedMessage(received_message);
    
    // Get file size
    UART_sendd("AT+HTTPREAD?\r\n");
    received_message = UART_receive(5000);
    if (received_message) {
        get_len = extract_data(received_message, "+HTTPREAD: ", "\r\n");
        freeReceivedMessage(received_message);
    }
    
    if (!get_len) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to get file size");
        goto cleanup;
    }
    
    file_size = atol(get_len);
    ESP_LOGI(TAG_SIM7600_HTTP, "Total file size: %d bytes", file_size);
    free(get_len);
    
    if (file_size == 0) {
        ESP_LOGE(TAG_SIM7600_HTTP, "File size is 0 - download failed");
        goto cleanup;
    }
    
    // Download in chunks
    size_t offset = 0;
    uint8_t *chunk_buffer = malloc(chunk_size);
    if (!chunk_buffer) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to allocate chunk buffer");
        goto cleanup;
    }
    
    while (offset < file_size) {
        // Feed watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
        
        size_t remaining = file_size - offset;
        size_t this_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        
        ESP_LOGD(TAG_SIM7600_HTTP, "Reading chunk at offset %d, size %d", offset, this_chunk);
        
        // Read chunk
        char read_cmd[SIM7600_CMD_MAX_LEN];
        snprintf(read_cmd, sizeof(read_cmd), "AT+HTTPREAD=%d,%d\r\n", offset, this_chunk);
        
        UART_sendd(read_cmd);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Read response
        size_t bytes_read = 0;
        int timeout = 0;
        int max_timeout = 30;
        
        while (bytes_read < this_chunk && timeout < max_timeout) {
            vTaskDelay(pdMS_TO_TICKS(10));
            
            char *response = UART_receive(2000);
            if (response) {
                size_t resp_len = strlen(response);
                if (resp_len > 0) {
                    size_t copy_len = (resp_len < (this_chunk - bytes_read)) ? resp_len : (this_chunk - bytes_read);
                    memcpy(chunk_buffer + bytes_read, response, copy_len);
                    bytes_read += copy_len;
                    ESP_LOGD(TAG_SIM7600_HTTP, "Read %d bytes, total %d/%d", copy_len, bytes_read, this_chunk);
                }
                freeReceivedMessage(response);
                timeout = 0;
            } else {
                timeout++;
            }
        }
        
        if (bytes_read > 0) {
            total_downloaded += bytes_read;
            
            if (callback) {
                if (!callback(chunk_buffer, bytes_read, total_downloaded, file_size)) {
                    ESP_LOGI(TAG_SIM7600_HTTP, "Download aborted by callback");
                    break;
                }
            }
            
            offset += bytes_read;
            ESP_LOGI(TAG_SIM7600_HTTP, "Downloaded %d/%d bytes (%d%%)", 
                     total_downloaded, file_size, (total_downloaded * 100 / file_size));
        } else {
            ESP_LOGE(TAG_SIM7600_HTTP, "Failed to read chunk at offset %d", offset);
            break;
        }
    }
    
    free(chunk_buffer);
    ret = (total_downloaded == file_size) ? ESP_OK : ESP_FAIL;

cleanup:
    ESP_LOGI(TAG_SIM7600_HTTP, "Cleaning up HTTP...");
    UART_sendd("AT+HTTPTERM\r\n");
    received_message = UART_receive(2000);
    if (received_message) freeReceivedMessage(received_message);
    
    return ret;
}
/*============================================================================
 * Example: Download and Save to SPIFFS
 *============================================================================*/

bool save_chunk_to_file(uint8_t *chunk, size_t chunk_size, 
                        size_t total_downloaded, size_t total_size) {
    if (!downloaded_file) return false;
    
    size_t written = fwrite(chunk, 1, chunk_size, downloaded_file);
    if (written != chunk_size) {
        ESP_LOGE("DOWNLOAD", "Failed to write to file");
        return false;
    }
    
    // Flush periodically
    if (total_downloaded % (64 * 1024) == 0) {
        fflush(downloaded_file);
    }
    
    return true;
}

esp_err_t download_firmware_to_spiffs(const char *url, const char *save_path) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Downloading firmware to %s", save_path);
    
    // Initialize SPIFFS first
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("DOWNLOAD", "Failed to mount SPIFFS");
        return ret;
    }
    
    // Open file for writing
    downloaded_file = fopen(save_path, "wb");
    if (!downloaded_file) {
        ESP_LOGE("DOWNLOAD", "Failed to open file for writing");
        esp_vfs_spiffs_unregister(NULL);
        return ESP_FAIL;
    }
    
    // Download with smaller chunks to prevent watchdog issues
    ret = SIMcom_download_from_github_chunked(url, 2048, save_chunk_to_file); // Reduced from 4096 to 2048
    
    // Cleanup
    fclose(downloaded_file);
    downloaded_file = NULL;
    esp_vfs_spiffs_unregister(NULL);
    
    if (ret == ESP_OK) {
        ESP_LOGI("DOWNLOAD", "Firmware saved to %s", save_path);
    } else {
        ESP_LOGE("DOWNLOAD", "Firmware download failed");
    }
    
    return ret;
}
/*============================================================================
 * High-Level Helper Functions
 *============================================================================*/

esp_err_t SIMcom_https_upload_to_certified_server(char *server, int port, char *url, char *json_str) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Starting HTTPS upload to certified server");
    
    // Check if HTTPS is available
    if (!SIMcom_check_https_available()) {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTPS not available");
        return ESP_FAIL;
    }
    
    // GitHub certificate
    const char *ca_cert = "-----BEGIN CERTIFICATE-----\n"
    "MIID6zCCA5KgAwIBAgIQHcKJwera+wTp0c9T1dciUzAKBggqhkjOPQQDAjBgMQsw\n"
    "CQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5T\n"
    "ZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gQ0EgRFYgRTM2MB4X\n"
    "DTI2MDMwNjAwMDAwMFoXDTI2MDYwMzIzNTk1OVowFTETMBEGA1UEAxMKZ2l0aHVi\n"
    "LmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABD1Kffi++z3kbHMsxQPnF+t2\n"
    "Rp5FxtVsskEqMsia5XnrWkhNBclCLbJja3rCdcQZSel6JzMH9wgmZZ8SfDguVlqj\n"
    "ggJ3MIICczAfBgNVHSMEGDAWgBQXmagEwW/kLXCoChA9A9PpGrgmYzAdBgNVHQ4E\n"
    "FgQUCBOC0QS5fQBrorGBIoPHx9xPjNwwDgYDVR0PAQH/BAQDAgeAMAwGA1UdEwEB\n"
    "/wQCMAAwEwYDVR0lBAwwCgYIKwYBBQUHAwEwSQYDVR0gBEIwQDA0BgsrBgEEAbIx\n"
    "AQICBzAlMCMGCCsGAQUFBwIBFhdodHRwczovL3NlY3RpZ28uY29tL0NQUzAIBgZn\n"
    "gQwBAgEwgYQGCCsGAQUFBwEBBHgwdjBPBggrBgEFBQcwAoZDaHR0cDovL2NydC5z\n"
    "ZWN0aWdvLmNvbS9TZWN0aWdvUHVibGljU2VydmVyQXV0aGVudGljYXRpb25DQURW\n"
    "RTM2LmNydDAjBggrBgEFBQcwAYYXaHR0cDovL29jc3Auc2VjdGlnby5jb20wggED\n"
    "BgorBgEEAdZ5AgQCBIH0BIHxAO8AdgAOV5S8866pPjMbLJkHs/eQ35vCPXEyJd0h\n"
    "qSWsYcVOIQAAAZzAgrmqAAAEAwBHMEUCIH/3H7cyzRR9nGWBnRchBzhuP2/3xe+I\n"
    "nenUGa7fw1nsAiEAkk+7dwWZeuBnn3AE70akZjc0hPFjW0Io4PfyeYeyZqoAdQAW\n"
    "gy2r8KklDw/wOqVF/8i/yCPQh0v2BCkn+OcfMxP1+gAAAZzAgrjLAAAEAwBGMEQC\n"
    "IAQ8uLtAPjjpu+9i2+q8I0oi4rtsYQp6O7H6xGmd5uuHAiAtmdU/0E7RDlnPOT7K\n"
    "c9ErIDvCCJsNAK/d6jZiZT04XzAlBgNVHREEHjAcggpnaXRodWIuY29tgg53d3cu\n"
    "Z2l0aHViLmNvbTAKBggqhkjOPQQDAgNHADBEAiAsxMOqDZ/76XvvnDWMHL2VLAuf\n"
    "9zWCT9sDfGGBWj6xDwIgWKRSMeJQGI6Q8kLHpLNGAhXKc0cnSy9lPxeZ5+Fe9LA=\n"
    "-----END CERTIFICATE-----\n";
    
    // Delete old certificates
    SIMcom_delete_certificate("ca.pem");
    
    // Download certificates
    if (SIMcom_download_certificate("ca.pem", ca_cert) != ESP_OK) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to download CA certificate");
        return ESP_FAIL;
    }
    
    // List certificates
    SIMcom_list_certificates();
    
    // Configure SSL
    if (SIMcom_configure_ssl("ca.pem", NULL, NULL, SSL_AUTH_SERVER) != ESP_OK) {
        ESP_LOGE(TAG_SIM7600_HTTP, "Failed to configure SSL");
        return ESP_FAIL;
    }
    
    // Perform HTTPS POST
    esp_err_t ret = SIMcom_https_post_SIM7600(server, port, url, json_str, true);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_SIM7600_HTTP, "HTTPS upload completed");
    } else {
        ESP_LOGE(TAG_SIM7600_HTTP, "HTTPS upload failed");
    }
    
    return ret;
}

bool SIMcom_check_https_available(void) {
    ESP_LOGI(TAG_SIM7600_HTTP, "Checking HTTPS availability");
    
    UART_sendd("AT+HTTPSSL?\r\n");
    char *response = UART_receive(1000);
    
    bool available = false;
    if (response) {
        if (strstr(response, "+HTTPSSL:") != NULL) {
            available = true;
        }
        freeReceivedMessage(response);
    }
    
    return available;
}

const char* SIMcom_get_last_ssl_error(void) {
    return last_ssl_error;
}

// Forward declaration for get_firmware_size (you need to implement this)
static size_t get_firmware_size(const char *url) {
    // This is a placeholder - you need to implement this function
    // to get the firmware size before downloading
    (void)url; // Suppress unused parameter warning
    ESP_LOGW(TAG_SIM7600_HTTP, "get_firmware_size not implemented, returning 0");
    return 0;
}

void download_firmware_safe(const char *url) {
    if (!url || url[0] == '\0') {
        ESP_LOGE(TAG_SIM7600_HTTP, "download_firmware_safe: no URL provided");
        return;
    }

    ESP_LOGI(TAG_SIM7600_HTTP, "Starting OTA download from: %s", url);

    // Check available heap
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI("MEMORY", "Free heap: %d bytes", free_heap);

    download_firmware_to_spiffs(url, "/spiffs/firmware.bin");
}