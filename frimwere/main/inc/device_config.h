/*
 * device_config.h
 * Runtime configuration received from the platform over MQTT.
 * Persisted in NVS so it survives reboots.
 */
#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TANK_SHAPE_MAX_LEN  32
#define TANK_SECTIONS_MAX   8

typedef enum {
    TANK_RECTANGULAR = 0,
    TANK_CYLINDER_VERTICAL,
    TANK_CYLINDER_HORIZONTAL,
    TANK_CONE_VERTICAL,
    TANK_ELLIPSE_VERTICAL,
    TANK_SPHERE,
    TANK_CAPSULE,
    TANK_MULTI_SECTION,
} tank_shape_e;

typedef struct {
    float length_m;
    float width_m;
    float height_m;
    float radius_m;
    float radius_b_m;
} tank_shape_params_t;

typedef struct {
    tank_shape_e        shape;
    tank_shape_params_t params;
} tank_section_t;

typedef struct {
    uint32_t            reporting_interval_s;   /* publish period (seconds)       */
    int                 timezone_offset_min;    /* UTC offset in minutes          */
    bool                gps_enabled;            /* run GPS if true                */
    bool                debug_mode;             /* publish logs topic if true     */
    tank_shape_e        tank_shape;
    char                tank_shape_str[TANK_SHAPE_MAX_LEN];
    tank_shape_params_t tank_params;
    tank_section_t      tank_sections[TANK_SECTIONS_MAX];
    int                 tank_section_count;
} device_config_t;

/* Single global instance — include this header and use g_device_config */
extern device_config_t g_device_config;

/* Load factory defaults into g_device_config */
void device_config_load_defaults(void);

/* Persist current g_device_config to NVS */
esp_err_t device_config_save_to_nvs(void);

/* Restore from NVS (calls load_defaults first so fields always valid) */
esp_err_t device_config_load_from_nvs(void);

/* Parse a config JSON string (from MQTT), apply to g_device_config, save */
esp_err_t device_config_apply_from_json(const char *json_str);

/* Serialize current g_device_config to a JSON string — caller must free() */
char *device_config_to_json_str(void);

/* Tank geometry helpers used by Sensor_Fuel_Manager */
float device_config_get_tank_height_cm(void);      /* max measurable height */
float device_config_calc_volume_l(float level_pct); /* volume at level_pct  */
float device_config_calc_capacity_l(void);          /* full tank capacity   */

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_CONFIG_H */
