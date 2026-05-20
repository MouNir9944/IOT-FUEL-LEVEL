/**
 * @file SIM7600_HTTP.h
 * @brief Driver for SIMCOM SIM7600 module HTTP/HTTPS operations with certificate authentication
 * 
 * This driver provides functions for HTTP/HTTPS GET/POST requests with SSL/TLS
 * and client certificate authentication using the SIM7600 module.
 * 
 * @author Your Name
 * @date 2024
 */

#ifndef SIM7600_HTTP_H
#define SIM7600_HTTP_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default delay constants for UART operations (in milliseconds)
 */
#define SIM7600_DELAY_SHORT   500
#define SIM7600_DELAY_MEDIUM  1000
#define SIM7600_DELAY_LONG    3000
#define SIM7600_DELAY_EXTRA   10000

/**
 * @brief Maximum buffer sizes
 */
#define SIM7600_URL_MAX_LEN    200
#define SIM7600_CMD_MAX_LEN    200
#define SIM7600_RESP_MAX_LEN   4096
#define SIM7600_CERT_MAX_LEN   4096

/**
 * @brief SSL Authentication modes
 */
typedef enum {
    SSL_AUTH_NONE = 0,           /**< No authentication */
    SSL_AUTH_SERVER = 1,          /**< Server authentication only */
    SSL_AUTH_MUTUAL = 2           /**< Mutual authentication (server + client) */
} ssl_auth_mode_t;

/**
 * @brief HTTP method types
 */
typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_PUT = 2,
    HTTP_METHOD_DELETE = 3
} http_method_t;

/**
 * @brief SSL/TLS version configuration
 */
typedef enum {
    SSL_VERSION_TLS10 = 1,        /**< TLS 1.0 only */
    SSL_VERSION_TLS11 = 2,        /**< TLS 1.1 only */
    SSL_VERSION_TLS12 = 3,        /**< TLS 1.2 only */
    SSL_VERSION_ALL = 4            /**< All TLS versions (1.0/1.1/1.2) */
} ssl_version_t;

/*============================================================================
 * Basic HTTP Functions
 *============================================================================*/

/**
 * @brief Perform HTTP GET request using SIM7600 module
 * 
 * @param server Server hostname or IP address
 * @param port Server port number
 * @param url Resource path/URL
 * @return ESP_OK on success, otherwise ESP_FAIL
 */
esp_err_t SIMcom_http_get_SIM7600(char *server, int port, char *url);

/**
 * @brief Perform HTTP POST request with JSON data using SIM7600 module
 * 
 * @param server Server hostname or IP address
 * @param port Server port number
 * @param url Resource path/URL
 * @param json_str JSON string to send in POST body
 * @return ESP_OK on success, otherwise ESP_FAIL
 */
esp_err_t SIMcom_http_post_SIM7600(char *server, int port, char *url, char *json_str);

/**
 * @brief Perform HTTP POST request with JSON data and process update response
 * 
 * @param server Server hostname or IP address
 * @param port Server port number
 * @param url Resource path/URL
 * @param json_str JSON string to send in POST body
 * @return ESP_OK on success, otherwise ESP_FAIL
 */
esp_err_t SIMcom_http_post_SIM7600_update(char *server, int port, char *url, char *json_str);

/*============================================================================
 * Certificate Management Functions
 *============================================================================*/

/**
 * @brief List all certificates stored in the module
 * 
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_list_certificates(void);

/**
 * @brief Delete a certificate from the module
 * 
 * @param filename Name of the certificate file to delete
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_delete_certificate(const char *filename);

/**
 * @brief Download a certificate to the module
 * 
 * @param filename Name to save the certificate as
 * @param cert_content The certificate content as string
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_download_certificate(const char *filename, const char *cert_content);

/**
 * @brief Download certificate from a file path (loads from SPIFFS)
 * 
 * @param filename Name to save in module
 * @param filepath Path to certificate file in SPIFFS
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_download_certificate_from_file(const char *filename, const char *filepath);

/*============================================================================
 * SSL/TLS Configuration Functions
 *============================================================================*/

/**
 * @brief Configure SSL context with certificates for HTTPS
 * 
 * @param ca_cert_filename Root CA certificate filename (NULL to skip)
 * @param client_cert_filename Client certificate filename (NULL to skip)
 * @param client_key_filename Client private key filename (NULL to skip)
 * @param auth_mode Authentication mode (0=no auth, 1=server only, 2=mutual)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_configure_ssl(const char *ca_cert_filename, 
                               const char *client_cert_filename,
                               const char *client_key_filename,
                               ssl_auth_mode_t auth_mode);

/**
 * @brief Set SSL/TLS version
 * 
 * @param ssl_version SSL version to use
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_set_ssl_version(ssl_version_t ssl_version);

/**
 * @brief Enable or disable Server Name Indication (SNI)
 * 
 * @param enable true to enable, false to disable
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_set_sni(bool enable);

/**
 * @brief Ignore SSL certificate validation (for testing only)
 * 
 * @param ignore true to ignore, false to validate
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_ignore_ssl_validation(bool ignore);

/**
 * @brief Clear all SSL configuration
 * 
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_clear_ssl_config(void);

/*============================================================================
 * HTTPS Functions with Certificate Authentication
 *============================================================================*/

/**
 * @brief Perform HTTPS GET request with SSL/TLS
 * 
 * @param server Server hostname
 * @param port Server port (usually 443)
 * @param url Resource path
 * @param use_auth Whether to use client certificate authentication
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_https_get_SIM7600(char *server, int port, char *url, bool use_auth);

/**
 * @brief Perform HTTPS POST request with SSL/TLS
 *
 * @param server Server hostname
 * @param port Server port (usually 443)
 * @param url Resource path
 * @param json_str JSON data to send
 * @param use_auth Whether to use client certificate authentication
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_https_post_SIM7600(char *server, int port, char *url, char *json_str, bool use_auth);

/**
 * @brief Perform HTTPS POST as multipart/form-data with JSON as the "file" field.
 *
 * This is required by the /api/devices/sync endpoint which expects:
 *   Content-Type: multipart/form-data
 *   Body field name: "file"  (JSON content)
 *
 * The function builds the multipart body internally from json_data, waits
 * correctly for the modem DOWNLOAD prompt and OK, then issues HTTPACTION=1
 * polling until the +HTTPACTION URC arrives.
 *
 * @param server    Server hostname (e.g. "smart-gridix-backend.onrender.com")
 * @param port      Server port (443 for HTTPS)
 * @param url       Resource path (e.g. "/api/devices/sync")
 * @param json_data Raw JSON string to upload as the file body
 * @param use_auth  Whether to use client-certificate authentication
 * @return ESP_OK on HTTP 2xx, ESP_FAIL otherwise
 */
esp_err_t SIMcom_https_post_multipart_SIM7600(char *server, int port, char *url,
                                               const char *json_data, bool use_auth);

/**
 * @brief Perform HTTPS request with custom method
 * 
 * @param server Server hostname
 * @param port Server port
 * @param url Resource path
 * @param method HTTP method (GET/POST/PUT/DELETE)
 * @param data Data to send (NULL for GET/DELETE)
 * @param use_auth Whether to use client certificate authentication
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_https_request_SIM7600(char *server, int port, char *url, 
                                       http_method_t method, char *data, bool use_auth);

/**
 * @brief Upload file to HTTPS server with certificate authentication
 * 
 * @param server Server hostname
 * @param port Server port
 * @param url Upload endpoint URL
 * @param file_data File data to upload
 * @param data_len Length of file data
 * @param content_type MIME content type
 * @param use_auth Whether to use client certificate authentication
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_https_upload_file(char *server, int port, char *url,
                                   char *file_data, size_t data_len,
                                   char *content_type, bool use_auth);

/*============================================================================
 * Download Functions with Progress Callback
 *============================================================================*/

/**
 * @brief Callback function type for download progress
 * @param chunk_data Pointer to the downloaded chunk
 * @param chunk_size Size of the chunk
 * @param total_downloaded Total bytes downloaded so far
 * @param total_size Total file size (0 if unknown)
 * @return true to continue, false to abort
 */
typedef bool (*download_callback_t)(uint8_t *chunk_data, size_t chunk_size, 
                                     size_t total_downloaded, size_t total_size);

/**
 * @brief Download file from GitHub in chunks with callback
 * @param github_raw_url Full raw GitHub URL
 * @param chunk_size Size of each chunk to process (e.g., 4096 bytes)
 * @param callback Function to call with each chunk
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_download_from_github_chunked(const char *github_raw_url, 
                                              size_t chunk_size,
                                              download_callback_t callback);

/**
 * @brief Save chunk to file callback function
 * @param chunk Pointer to the downloaded chunk
 * @param chunk_size Size of the chunk
 * @param total_downloaded Total bytes downloaded so far
 * @param total_size Total file size
 * @return true to continue, false to abort
 */
bool save_chunk_to_file(uint8_t *chunk, size_t chunk_size, 
                        size_t total_downloaded, size_t total_size);

/**
 * @brief Download firmware and save to SPIFFS
 * @param url The GitHub raw URL to download from
 * @param save_path Path in SPIFFS to save the file (e.g., "/spiffs/firmware.bin")
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t download_firmware_to_spiffs(const char *url, const char *save_path);

/**
 * @brief Safe firmware download that checks memory and chooses appropriate method
 * @param url Full URL to download firmware from (HTTP or HTTPS)
 */
void download_firmware_safe(const char *url);

/*============================================================================
 * High-Level Helper Functions
 *============================================================================*/

/**
 * @brief Complete example of uploading to a certified HTTPS server
 * 
 * @param server Server hostname
 * @param port Server port
 * @param url Upload endpoint URL
 * @param json_str JSON data to upload
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t SIMcom_https_upload_to_certified_server(char *server, int port, char *url, char *json_str);

/**
 * @brief Check if HTTPS is supported and certificates are loaded
 * 
 * @return true if HTTPS is available, false otherwise
 */
bool SIMcom_check_https_available(void);

/**
 * @brief Get last SSL error message
 * 
 * @return const char* Error message or NULL
 */
const char* SIMcom_get_last_ssl_error(void);
void SIMcom_debug_certificate_download(const char *filename, const char *cert_content);
void SIMcom_test_certificate_download(const char *filename);
void SIMcom_test_url_setting(const char *url) ;
#ifdef __cplusplus
}
#endif

#endif /* SIM7600_HTTP_H */