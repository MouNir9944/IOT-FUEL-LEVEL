#ifndef SIM7600_MQTT_H
#define SIM7600_MQTT_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT Client States */
typedef enum {
    MQTT_STATE_IDLE = 0,
    MQTT_STATE_STARTED,
    MQTT_STATE_CLIENT_ACQUIRED,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_SUBSCRIBED,
    MQTT_STATE_DISCONNECTED
} sim7600_mqtt_state_t;

/* MQTT QoS Levels */
typedef enum {
    MQTT_QOS_0 = 0,  /* At most once */
    MQTT_QOS_1 = 1,  /* At least once */
    MQTT_QOS_2 = 2   /* Exactly once */
} sim7600_mqtt_qos_t;

/* MQTT Client Handle */
typedef struct {
    uint8_t client_index;           /* 0 or 1 */
    char client_id[32];              /* Up to 31 bytes + null */
    sim7600_mqtt_state_t state;
    char broker_url[257];            /* Max 256 bytes + null */
    uint16_t keepalive;              /* 60-64800 seconds */
    uint8_t clean_session;           /* 0 or 1 */
    uint8_t ssl_ctx_index;           /* 0-9 for SSL/TLS */
    uint32_t last_activity;
    bool ssl_enabled;
} sim7600_mqtt_client_t;

/* MQTT Message Callback Type 
 * Called when a message is received on a subscribed topic
 */
typedef void (*sim7600_mqtt_message_callback_t)(uint8_t client_index, const char* topic, const char* payload, size_t payload_len);

/* MQTT Event Callback Type 
 * Called when MQTT state changes or errors occur
 */
typedef void (*sim7600_mqtt_event_callback_t)(uint8_t client_index, sim7600_mqtt_state_t state, esp_err_t error);

/*=============================================================================
 * Initialization and Deinitialization
 *============================================================================*/

/**
 * @brief Initialize the MQTT subsystem
 * 
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_init(void);

/**
 * @brief Deinitialize the MQTT subsystem and release all resources
 * 
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_deinit(void);

/*=============================================================================
 * Client Management
 *============================================================================*/

/**
 * @brief Acquire an MQTT client
 * 
 * @param client Pointer to client structure to initialize
 * @param client_id Unique client identifier (1-23 bytes)
 * @param ssl_enabled true for SSL/TLS connection, false for TCP
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_client_acquire(sim7600_mqtt_client_t* client, const char* client_id, bool ssl_enabled);

/**
 * @brief Release an MQTT client
 * 
 * @param client Pointer to client structure
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_client_release(sim7600_mqtt_client_t* client);

/*=============================================================================
 * MQTT Service Management
 *============================================================================*/

/**
 * @brief Start MQTT service (activate PDP context)
 * 
 * @param client Pointer to client structure
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_start(sim7600_mqtt_client_t* client);

/**
 * @brief Stop MQTT service (deactivate PDP context)
 * 
 * @param client Pointer to client structure
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_stop(sim7600_mqtt_client_t* client);

/*=============================================================================
 * Will Message Configuration
 *============================================================================*/

/**
 * @brief Set will message (last testament)
 * 
 * @param client Pointer to client structure
 * @param topic Will topic
 * @param message Will message
 * @param qos QoS level (0-2)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_set_will(sim7600_mqtt_client_t* client, const char* topic, const char* message, uint8_t qos);

/*=============================================================================
 * Connection Functions (Updated with separate host/port)
 *============================================================================*/

/**
 * @brief Connect to MQTT broker with separate host and port
 * 
 * @param client Pointer to client structure
 * @param host Broker hostname or IP address (e.g., "test.mosquitto.org")
 * @param port Broker port (e.g., 1883 for TCP, 8883 for SSL)
 * @param keepalive Keepalive interval in seconds (60-64800)
 * @param clean_session Clean session flag (0=persistent, 1=clean)
 * @param username Username for authentication (can be NULL)
 * @param password Password for authentication (can be NULL)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_connect(sim7600_mqtt_client_t* client, 
                               const char* host, 
                               uint16_t port, 
                               uint16_t keepalive, 
                               uint8_t clean_session,
                               const char* username, 
                               const char* password);

/**
 * @brief Connect to MQTT broker with default TCP port (1883)
 * 
 * @param client Pointer to client structure
 * @param host Broker hostname or IP address
 * @param keepalive Keepalive interval in seconds (60-64800)
 * @param clean_session Clean session flag (0=persistent, 1=clean)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_connect_default(sim7600_mqtt_client_t* client, 
                                       const char* host, 
                                       uint16_t keepalive, 
                                       uint8_t clean_session);

/**
 * @brief Connect to SSL/TLS MQTT broker with default SSL port (8883)
 * 
 * @param client Pointer to client structure (must have ssl_enabled=true)
 * @param host Broker hostname or IP address
 * @param keepalive Keepalive interval in seconds (60-64800)
 * @param clean_session Clean session flag (0=persistent, 1=clean)
 * @param username Username for authentication (can be NULL)
 * @param password Password for authentication (can be NULL)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_connect_ssl(sim7600_mqtt_client_t* client, 
                                   const char* host, 
                                   uint16_t keepalive, 
                                   uint8_t clean_session,
                                   const char* username, 
                                   const char* password);

/**
 * @brief Disconnect from MQTT broker
 * 
 * @param client Pointer to client structure
 * @param timeout_sec Disconnect timeout in seconds (60-180)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_disconnect_broker(sim7600_mqtt_client_t* client, uint16_t timeout_sec);

/*=============================================================================
 * Publish Functions
 *============================================================================*/

/**
 * @brief Publish a message to a topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to publish to
 * @param payload Message payload (string)
 * @param qos QoS level (0-2)
 * @param retained Retained message flag
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_publish(sim7600_mqtt_client_t* client, 
                               const char* topic, 
                               const char* payload, 
                               uint8_t qos, 
                               bool retained);
/**
 * @brief Publish a message to a topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to publish to
 * @param payload Message payload (string)
 * @param qos QoS level (0-2)
 * @param retained Retained message flag
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_publish_data(sim7600_mqtt_client_t* client, 
                               const char* topic, 
                               const char* payload, 
                               uint8_t qos, 
                               bool retained);
/**
 * @brief Publish a message to a topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to publish to
 * @param payload Message payload (string)
 * @param qos QoS level (0-2)
 * @param retained Retained message flag
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_publish_log(sim7600_mqtt_client_t* client, 
                               const char* topic, 
                               const char* payload, 
                               uint8_t qos, 
                               bool retained);                              
/**
 * @brief Publish binary data to a topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to publish to
 * @param data Binary data buffer
 * @param len Length of data in bytes
 * @param qos QoS level (0-2)
 * @param retained Retained message flag
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_publish_hex(sim7600_mqtt_client_t* client, 
                                   const char* topic, 
                                   const uint8_t* data, 
                                   size_t len, 
                                   uint8_t qos, 
                                   bool retained);

/*=============================================================================
 * Subscribe Functions
 *============================================================================*/

/**
 * @brief Subscribe to a single topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to subscribe to
 * @param qos Requested QoS level (0-2)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_subscribe(sim7600_mqtt_client_t* client, 
                                 const char* topic, 
                                 uint8_t qos);

/**
 * @brief Subscribe to multiple topics in one command
 * 
 * @param client Pointer to client structure
 * @param topics Array of topic strings
 * @param qos Array of QoS levels for each topic
 * @param count Number of topics (max 10)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_subscribe_multiple(sim7600_mqtt_client_t* client, 
                                          const char* topics[], 
                                          uint8_t* qos, 
                                          size_t count);

/**
 * @brief Unsubscribe from a topic
 * 
 * @param client Pointer to client structure
 * @param topic Topic to unsubscribe from
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_unsubscribe(sim7600_mqtt_client_t* client, 
                                   const char* topic);

/*=============================================================================
 * Configuration Functions
 *============================================================================*/

/**
 * @brief Enable or disable UTF-8 string checking
 * 
 * @param client Pointer to client structure
 * @param enable true to enable UTF-8 checking, false to disable
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_set_utf8_check(sim7600_mqtt_client_t* client, bool enable);

/**
 * @brief Set SSL context for SSL/TLS connection
 * 
 * @param client Pointer to client structure (must have ssl_enabled=true)
 * @param ssl_ctx_index SSL context index (0-9)
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t sim7600_mqtt_set_ssl_context(sim7600_mqtt_client_t* client, uint8_t ssl_ctx_index);

/*=============================================================================
 * Callback Registration
 *============================================================================*/

/**
 * @brief Register callback for received MQTT messages
 * 
 * @param callback Function pointer to message callback
 */
void sim7600_mqtt_register_message_callback(sim7600_mqtt_message_callback_t callback);

/**
 * @brief Register callback for MQTT state change events
 * 
 * @param callback Function pointer to event callback
 */
void sim7600_mqtt_register_event_callback(sim7600_mqtt_event_callback_t callback);

/*=============================================================================
 * URC Processing
 *============================================================================*/

/**
 * @brief Process unsolicited result codes from modem
 * Call this function when URC data is received from the modem
 * 
 * @param line The URC line received from modem
 */
void sim7600_mqtt_process_urc(const char* line);

/*=============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Convert MQTT state enum to string
 * 
 * @param state MQTT state
 * @return String representation of the state
 */
const char* sim7600_mqtt_state_to_str(sim7600_mqtt_state_t state);

/**
 * @brief Get error description from error code
 * 
 * @param err_code Error code from MQTT commands
 * @return String description of the error
 */
const char* sim7600_mqtt_error_to_str(int err_code);

/*=============================================================================
 * Legacy Support (for backward compatibility)
 *============================================================================*/

/**
 * @brief Legacy connect function with full URL (maintained for compatibility)
 * 
 * @deprecated Use sim7600_mqtt_connect with separate host/port instead
 */
esp_err_t SIM7600_connect_mqtt(const char* client_id, const char* broker_url, 
                               const char* will_topic, const char* will_msg, 
                               const char* sub_topic, const char* pub_topic, 
                               const char* pub_payload) __attribute__((deprecated));

#ifdef __cplusplus
}
#endif

#endif /* SIM7600_MQTT_H */