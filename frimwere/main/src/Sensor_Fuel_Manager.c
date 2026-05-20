#include "Sensor_Fuel_Manager.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modem_event_handler.h"
#include "SIM7600.h"
#include "device_config.h"

static const char *TAG_SENSOR_FUEL = "Sensor Manager";
static const char *TAG_SOJI_UART   = "SOJI UART";
extern modem_signal_event_t signal_quality;
extern gps_coordinates_t    current_gps;

/* ── SOJI UART Handle ─────────────────────────────────────────────────────── */
typedef struct {
    uart_port_t  uart_num;
    gpio_num_t   tx_pin;
    gpio_num_t   rx_pin;
    gpio_num_t   de_re_pin;   /* DE/RE — wired to UART RTS for half-duplex */
} soji_uart_handle_t;

static soji_uart_handle_t s_soji_uart = {
    .uart_num  = UART_NUM_2,
    .tx_pin    = TXD_PIN_2,
    .rx_pin    = RXD_PIN_2,
    .de_re_pin = DERE,
};

/* ── Battery ADC (GPIO 39 = ADC1 Channel 7) ──────────────────────────────────
 * Assumes a /2 voltage divider on the battery line (Vbat → 100K → GPIO39
 * → 100K → GND).  Adjust BAT_VDIV_RATIO if your circuit differs.           */
#define BAT_ADC_CHANNEL   ADC_CHANNEL_7   /* GPIO 39                          */
#define BAT_ADC_ATTEN     ADC_ATTEN_DB_12 /* 0 – ~3.1 V input range           */
#define BAT_VDIV_RATIO    2               /* multiply ADC mV by this factor   */
#define BAT_SAMPLES       8               /* averaged reads per measurement   */

static adc_oneshot_unit_handle_t s_bat_adc_handle = NULL;
static adc_cali_handle_t         s_bat_adc_cali   = NULL;
static bool                      s_bat_adc_ready  = false;

/* ── Version Definitions ─────────────────────────────────────────────────── */
#define FIRMWARE_VERSION   "v1.0.0"
#define HARDWARE_VERSION   "v1.0"

/* ── ADC Functions ───────────────────────────────────────────────────────── */
void bat_adc_init(void)
{
    if (s_bat_adc_handle) return;   /* already done */

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &s_bat_adc_handle) != ESP_OK) {
        ESP_LOGE(TAG_SENSOR_FUEL, "ADC unit init failed");
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = BAT_ADC_ATTEN,
    };
    if (adc_oneshot_config_channel(s_bat_adc_handle, BAT_ADC_CHANNEL, &chan_cfg) != ESP_OK) {
        ESP_LOGE(TAG_SENSOR_FUEL, "ADC channel config failed");
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cf_cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = BAT_ADC_CHANNEL,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cf_cfg, &s_bat_adc_cali) == ESP_OK) {
        ESP_LOGI(TAG_SENSOR_FUEL, "Battery ADC: curve-fitting calibration OK");
        s_bat_adc_ready = true;
        return;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t lf_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&lf_cfg, &s_bat_adc_cali) == ESP_OK) {
        ESP_LOGI(TAG_SENSOR_FUEL, "Battery ADC: line-fitting calibration OK");
        s_bat_adc_ready = true;
        return;
    }
#endif

    ESP_LOGW(TAG_SENSOR_FUEL, "Battery ADC: no calibration — using raw approximation");
    s_bat_adc_ready = true;
}

int read_battery_mv(void)
{
    if (!s_bat_adc_ready) return -1;

    int32_t sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_bat_adc_handle, BAT_ADC_CHANNEL, &raw) != ESP_OK) {
            return -1;
        }
        sum += raw;
    }

    int raw_avg = sum / BAT_SAMPLES;
    int adc_mv  = 0;

    if (s_bat_adc_cali) {
        adc_cali_raw_to_voltage(s_bat_adc_cali, raw_avg, &adc_mv);
    } else {
        adc_mv = (raw_avg * 3100) / 4095;
    }

    int battery_mv = adc_mv * BAT_VDIV_RATIO;

    if (battery_mv < 2500) battery_mv = 10;
    if (battery_mv > 4300) battery_mv = 4300;

    return battery_mv;
}

/* ── Static UART / RS-485 helpers ─────────────────────────────────────────── */

static esp_err_t soji_uart_init(soji_uart_handle_t *handle)
{
    uart_config_t cfg = {
        .baud_rate  = 9600,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err;

    err = uart_param_config(handle->uart_num, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SOJI_UART, "uart_param_config failed (0x%x)", err);
        return err;
    }

    /* RTS pin drives DE/RE; CTS unused */
    err = uart_set_pin(handle->uart_num,
                       handle->tx_pin, handle->rx_pin,
                       handle->de_re_pin, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SOJI_UART, "uart_set_pin failed (0x%x)", err);
        return err;
    }

    err = uart_driver_install(handle->uart_num,
                              UART_BUFFER_SIZE_2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SOJI_UART, "uart_driver_install failed (0x%x)", err);
        return err;
    }

    /* Half-duplex RS-485: driver toggles RTS (DE/RE) automatically */
    err = uart_set_mode(handle->uart_num, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SOJI_UART, "uart_set_mode RS485 failed (0x%x)", err);
        uart_driver_delete(handle->uart_num);
        return err;
    }

    return ESP_OK;
}

static esp_err_t soji_uart_send_data(soji_uart_handle_t *handle,
                                     uint8_t *data, int length)
{
    int written = uart_write_bytes(handle->uart_num, (const char *)data, length);
    if (written < 0) {
        ESP_LOGE(TAG_SOJI_UART, "uart_write_bytes failed");
        return ESP_FAIL;
    }
    /* Wait for TX FIFO to drain before the driver de-asserts DE/RE */
    uart_wait_tx_done(handle->uart_num, pdMS_TO_TICKS(100));
    return ESP_OK;
}

static int soji_uart_receive_data(soji_uart_handle_t *handle,
                                  uint8_t *buffer, int buffer_size,
                                  int timeout_ms)
{
    return uart_read_bytes(handle->uart_num, buffer, buffer_size,
                           pdMS_TO_TICKS(timeout_ms));
}

/* ── GPIO DE/RE helper (legacy / manual-control fallback) ─────────────────── */
void gpio_init_control_rs485(gpio_num_t gpio_rs485)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_rs485),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(gpio_rs485, 0);   /* start in receive mode */
}

/* ── Public UART init ─────────────────────────────────────────────────────── */
esp_err_t UART_init_SOJI(void)
{
    esp_err_t err = soji_uart_init(&s_soji_uart);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SENSOR_FUEL, "SOJI UART init failed (0x%x)", err);
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_LOGI(TAG_SENSOR_FUEL,
             "SOJI UART RS-485 ready  UART%d  TX=%d  RX=%d  DE/RE=%d  9600-8N1",
             s_soji_uart.uart_num, s_soji_uart.tx_pin,
             s_soji_uart.rx_pin,  s_soji_uart.de_re_pin);
    return ESP_OK;
}

/* ── SOJI Sensor Communication ────────────────────────────────────────────── */
uint8_t crc8(uint8_t data, uint8_t crc)
{
    uint8_t i = data ^ crc;
    crc = 0;
    if (i & 0x01) crc ^= 0x5e;
    if (i & 0x02) crc ^= 0xbc;
    if (i & 0x04) crc ^= 0x61;
    if (i & 0x08) crc ^= 0xc2;
    if (i & 0x10) crc ^= 0x9d;
    if (i & 0x20) crc ^= 0x23;
    if (i & 0x40) crc ^= 0x46;
    if (i & 0x80) crc ^= 0x8c;
    return crc;
}

uint8_t calculate_crc8_checksum(uint8_t data[], int length)
{
    uint8_t crc = 0;
    for (int i = 0; i < length - 1; i++) {
        crc = crc8(data[i], crc);
    }
    return crc;
}

void send_data_with_checksum(uint8_t data[], int length)
{
    ESP_LOGI(TAG_SENSOR_FUEL, "length: %d", length);
    uint8_t checksum = calculate_crc8_checksum(data, length);
    ESP_LOGI(TAG_SENSOR_FUEL, "checksum: %02X", checksum);
    data[length - 1] = checksum;

    soji_uart_send_data(&s_soji_uart, data, length);

    ESP_LOGI(TAG_SENSOR_FUEL, "Sent data with checksum:");
    for (int i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

cJSON *parse_response(uint8_t *data, size_t length)
{
    if (length < 9) {
        ESP_LOGE(TAG_SENSOR_FUEL, "Invalid response length");
        return NULL;
    }

    uint8_t  prefix          = data[0];
    uint8_t  network_address = data[1];
    uint8_t  operation_code  = data[2];
    int8_t   temperature     = (int8_t)data[3];
    /* Little-endian: low byte first */
    uint16_t relative_level  = ((uint16_t)data[5] << 8) | data[4];
    uint16_t frequency_value = ((uint16_t)data[7] << 8) | data[6];
    uint8_t  checksum        = data[8];

    if (prefix != 0x3E) {
        ESP_LOGE(TAG_SENSOR_FUEL, "Invalid prefix: expected 0x3E, got 0x%02X", prefix);
        return NULL;
    }
    if (operation_code != 0x06) {
        ESP_LOGE(TAG_SENSOR_FUEL, "Invalid op code: expected 0x06, got 0x%02X", operation_code);
        return NULL;
    }

    uint8_t calculated_checksum = calculate_crc8_checksum(data, 9);
    if (checksum != calculated_checksum) {
        ESP_LOGE(TAG_SENSOR_FUEL, "Checksum mismatch: rx 0x%02X calc 0x%02X",
                 checksum, calculated_checksum);
        return NULL;
    }

    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG_SENSOR_FUEL, "Failed to create JSON object");
        return NULL;
    }

    ESP_LOGI(TAG_SENSOR_FUEL, "Prefix: %02X  Addr: %02X  OpCode: %02X  Temp: %d  Level: %d  Freq: %d  CRC: %02X",
             prefix, network_address, operation_code, temperature,
             relative_level, frequency_value, checksum);

    cJSON_AddNumberToObject(response, "Address",     network_address);
    cJSON_AddNumberToObject(response, "Temperature", temperature);
    cJSON_AddNumberToObject(response, "Level",       relative_level);
    cJSON_AddNumberToObject(response, "Frequency",   frequency_value);

    return response;
}

cJSON *soji_sensor_fuel_level(uint8_t send_data[], int len)
{
    send_data_with_checksum(send_data, len);

    uint8_t recv_data[9];
    int length = soji_uart_receive_data(&s_soji_uart, recv_data,
                                        sizeof(recv_data), 2000);

    if (length > 0) {
        ESP_LOGI(TAG_SENSOR_FUEL, "Received data:");
        for (int i = 0; i < length; i++) {
            printf("%02X ", recv_data[i]);
        }
        printf("\n");
        return parse_response(recv_data, length);
    }

    ESP_LOGE(TAG_SENSOR_FUEL, "Failed to receive data from SOJI sensor");
    return NULL;
}

void create_and_send_json_request_soji_sensor_fuel_level(cJSON **payload_str)
{
    time_t now = 0;
    time(&now);
    struct tm timeinfo;

    char time_str[64];

    float level_mm = 0;
    float level_cm = 0;
    float level_pct = 0;
    float temp_c = 0;
    float offset_level_mm =25;
    uint16_t max_height_mm=(device_config_get_tank_height_cm()*10)+offset_level_mm;
    uint8_t send_data[] = {0x31, 0x01, 0x06, 0x00};
    cJSON *sensor_data  = soji_sensor_fuel_level(send_data, (int)sizeof(send_data));
    ESP_LOGE(TAG_SENSOR_FUEL, "max_height: %d", max_height_mm);
    if (sensor_data != NULL) {
        cJSON *level_relative = cJSON_GetObjectItem(sensor_data, "Level");
        cJSON *frequency_item = cJSON_GetObjectItem(sensor_data, "Frequency");
        cJSON *temp_item = cJSON_GetObjectItem(sensor_data, "Temperature");
    
        /*
    
        // Process Frequency (raw ADC value 0-25371)
        if (frequency_item && cJSON_IsNumber(frequency_item)) {
            int frequency = frequency_item->valueint;
            
            // Map frequency to level in mm
            // 0 = full tank (750mm), 25371 = empty (0mm)
            if (frequency <= 0) {
                level_mm = max_height_mm;  // 750 mm
            } else if (frequency >= 25371) {
                level_mm = 0.0;
            } else {
                // IMPORTANT: Use float division
                level_mm = max_height_mm * (1.0 - (float)frequency / 25371.0);
                //level_mm=+offset_level_mm;
            }
            
            level_cm = level_mm / 10.0;
            ESP_LOGE(TAG_SENSOR_FUEL, "frequency: %d", frequency);
            ESP_LOGE(TAG_SENSOR_FUEL, "level_mm: %.2f", level_mm);
            ESP_LOGE(TAG_SENSOR_FUEL, "level_cm: %.2f", level_cm);

        }
        */
        // Process Level (if Frequency not available, or as alternative)
         if (level_relative && cJSON_IsNumber(level_relative)) {
       
            int raw_level =  level_relative->valueint ;//* 100.0f;
            
            // Check the actual range from your debug: Level: 7500
            // 7500 seems to be the raw value, not 0-25371
            // You need to determine the correct mapping for your sensor
            
            // Option 1: If Level is already in mm (7500 = 750.0 mm)
            if (raw_level >=  7500) {
                level_mm = max_height_mm;  // Full
            } else if (raw_level <= 0) {
                level_mm = 0.0;  // Empty
            } else {
                // Map 0-6500 to 0-750 mm
                level_mm = max_height_mm * ((float)raw_level /  7500.0);
            }
            
            level_cm = level_mm / 10.0;         
            ESP_LOGE(TAG_SENSOR_FUEL, "level relative: %d", raw_level); 
            ESP_LOGE(TAG_SENSOR_FUEL, "level_mm: %.2f", level_mm);
            ESP_LOGE(TAG_SENSOR_FUEL, "level_cm: %.2f", level_cm);
        }
        
        // Process temperature
        if (temp_item && cJSON_IsNumber(temp_item)) {
            temp_c = temp_item->valueint;
            ESP_LOGE(TAG_SENSOR_FUEL, "temperature: %.2f", temp_c);
        }
        
        cJSON_Delete(sensor_data);
    } else {
        *payload_str = NULL;
        return;
    }

    /* Read signal quality and network mode synchronously so the values are
     * fresh in the JSON built below — modem_get_signal() only queues the
     * command and returns before it runs, giving a stale cached value.    */
    signal_quality.signal_dbm  = get_signal_quality();
    signal_quality.network_mode = get_network_mode();
    // Convertir en heure locale
    localtime_r(&now, &timeinfo);

    // Formater comme vous voulez
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGE(TAG_SENSOR_FUEL, "time/date: %s", time_str);
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        *payload_str = NULL;
        return;
    }
    ESP_LOGE(TAG_SENSOR_FUEL, "volume_l: %.1f", device_config_calc_volume_l(level_cm));
    ESP_LOGE(TAG_SENSOR_FUEL, "capacity_l: %.1f", device_config_calc_capacity_l());
    level_pct=(device_config_calc_volume_l(level_cm)/device_config_calc_capacity_l())*100;
    ESP_LOGE(TAG_SENSOR_FUEL, "level_pct: %.1f", level_pct);

    cJSON_AddNumberToObject(root, "level_pct",        (double)level_pct);
    cJSON_AddNumberToObject(root, "level_cm",         (double)level_cm);
    cJSON_AddNumberToObject(root, "volume_l",         (double)device_config_calc_volume_l(level_cm));
    cJSON_AddNumberToObject(root, "capacity_l",       (double)device_config_calc_capacity_l());
    cJSON_AddNumberToObject(root, "temp_c",           (double)temp_c);
    cJSON_AddNumberToObject(root, "battery_mv",       read_battery_mv());
    cJSON_AddNumberToObject(root, "rssi",             signal_quality.signal_dbm);
    cJSON_AddStringToObject(root, "network_mode", network_mode_to_str(signal_quality.network_mode));
    cJSON_AddStringToObject(root, "firmware_version", FIRMWARE_VERSION);
    cJSON_AddStringToObject(root, "hardware_version", HARDWARE_VERSION);
    cJSON_AddNumberToObject(root, "ts",               (double)(long)now);

    cJSON *gps = cJSON_CreateObject();
    if (gps) {
        cJSON_AddNumberToObject(gps, "lat",      current_gps.latitude);
        cJSON_AddNumberToObject(gps, "lng",      current_gps.longitude);
        cJSON_AddNumberToObject(gps, "alt",      current_gps.altitude);
        cJSON_AddNumberToObject(gps, "accuracy", 0.0);
        cJSON_AddItemToObject(root, "gps", gps);
    }

    *payload_str = root;
}
