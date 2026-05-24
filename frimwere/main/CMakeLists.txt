set(requires
    driver
    esp_driver_uart
    nvs_flash
    esp_event
    esp_netif
    log
    freertos
    esp_partition
    bootloader_support
    esp_app_format
    esp_system
    cJSON
    uart
    app_update          # Add THIS instead of esp_ota
)
set(requires "")
idf_build_get_property(target IDF_TARGET)

idf_component_register(SRCS "main.c"

                            "./Driver_modem/src/SIM7600.c"
                            "./Driver_modem/src/SIM7600_MQTT.c"
                            "./Driver_modem/src/SIMCOM_Driver.c"
                            "./Driver_modem/src/modem_event_handler.c"
                            "./Driver_modem/src/modem_mqtt_handler.c"
                            "./Driver_modem/src/SIM7600_HTTP.c"
                            "./src/LOG_Save_SD_Manager.c"
                            "./src/Sensor_Fuel_Manager.c"
                            "./src/nvs_manager.c"
                            "./src/device_config.c"

                    INCLUDE_DIRS "./inc"
                    "."
                    "./Driver_modem/inc"
                    REQUIRES ${requires})

# ── Auto-erase SPIFFS on every flash ─────────────────────────────────────────
# Builds a blank SPIFFS image from the empty spiffs_image/ directory and
# includes it in the normal "idf.py flash" target.  Every flash wipes the
# storage partition clean — useful during development to avoid stale readings.
#spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
