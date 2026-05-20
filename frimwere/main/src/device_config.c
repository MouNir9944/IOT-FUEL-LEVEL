/*
 * device_config.c
 */
#include "device_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_log.h"
#include "SIM7600.h"    /* set_esp32_timezone() */
#include <string.h>
#include <math.h>
#include <stdlib.h>

static const char *TAG_CFG = "DEVICE_CONFIG";

#define NVS_NAMESPACE  "dev_cfg"
#define NVS_KEY        "cfg_json"
#define NVS_MAX_LEN    2048

/* ── Global instance ───────────────────────────────────────────────────── */
device_config_t g_device_config;

/* ── String → enum ─────────────────────────────────────────────────────── */
static tank_shape_e shape_from_str(const char *s) {
    if (!s) return TANK_RECTANGULAR;
    if (strcmp(s, "rectangular")         == 0) return TANK_RECTANGULAR;
    if (strcmp(s, "cylinder_vertical")   == 0) return TANK_CYLINDER_VERTICAL;
    if (strcmp(s, "cylinder_horizontal") == 0) return TANK_CYLINDER_HORIZONTAL;
    if (strcmp(s, "cone_vertical")       == 0) return TANK_CONE_VERTICAL;
    if (strcmp(s, "ellipse_vertical")    == 0) return TANK_ELLIPSE_VERTICAL;
    if (strcmp(s, "sphere")              == 0) return TANK_SPHERE;
    if (strcmp(s, "capsule")             == 0) return TANK_CAPSULE;
    if (strcmp(s, "multi_section")       == 0) return TANK_MULTI_SECTION;
    return TANK_RECTANGULAR;
}

/* ── Defaults ───────────────────────────────────────────────────────────── */
void device_config_load_defaults(void) {
    g_device_config.reporting_interval_s = 30;
    g_device_config.timezone_offset_min  = 0;
    g_device_config.gps_enabled          = true;
    g_device_config.debug_mode           = false;
    g_device_config.tank_shape           = TANK_RECTANGULAR;
    strncpy(g_device_config.tank_shape_str, "rectangular", TANK_SHAPE_MAX_LEN - 1);
    g_device_config.tank_params.length_m   = 1.0f;
    g_device_config.tank_params.width_m    = 1.0f;
    g_device_config.tank_params.height_m   = 2.0f;
    g_device_config.tank_params.radius_m   = 0.5f;
    g_device_config.tank_params.radius_b_m = 0.4f;
    g_device_config.tank_section_count     = 0;
}

/* ── Shape params helper ────────────────────────────────────────────────── */
static void parse_shape_params(cJSON *obj, tank_shape_params_t *out) {
    if (!obj || !out) return;
    cJSON *it;
    if ((it = cJSON_GetObjectItem(obj, "length_m"))   && cJSON_IsNumber(it)) out->length_m   = (float)it->valuedouble;
    if ((it = cJSON_GetObjectItem(obj, "width_m"))    && cJSON_IsNumber(it)) out->width_m    = (float)it->valuedouble;
    if ((it = cJSON_GetObjectItem(obj, "height_m"))   && cJSON_IsNumber(it)) out->height_m   = (float)it->valuedouble;
    if ((it = cJSON_GetObjectItem(obj, "radius_m"))   && cJSON_IsNumber(it)) out->radius_m   = (float)it->valuedouble;
    if ((it = cJSON_GetObjectItem(obj, "radius_b_m")) && cJSON_IsNumber(it)) out->radius_b_m = (float)it->valuedouble;
}

/* ── NVS save ───────────────────────────────────────────────────────────── */
esp_err_t device_config_save_to_nvs(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "reporting_interval_s", g_device_config.reporting_interval_s);
    cJSON_AddNumberToObject(root, "timezone_offset_min",  g_device_config.timezone_offset_min);
    cJSON_AddBoolToObject  (root, "gps_enabled",          g_device_config.gps_enabled);
    cJSON_AddBoolToObject  (root, "debug_mode",           g_device_config.debug_mode);
    cJSON_AddStringToObject(root, "tank_shape",           g_device_config.tank_shape_str);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "length_m",   g_device_config.tank_params.length_m);
    cJSON_AddNumberToObject(p, "width_m",    g_device_config.tank_params.width_m);
    cJSON_AddNumberToObject(p, "height_m",   g_device_config.tank_params.height_m);
    cJSON_AddNumberToObject(p, "radius_m",   g_device_config.tank_params.radius_m);
    cJSON_AddNumberToObject(p, "radius_b_m", g_device_config.tank_params.radius_b_m);
    cJSON_AddItemToObject(root, "tank_shape_params", p);

    cJSON *secs = cJSON_CreateArray();
    for (int i = 0; i < g_device_config.tank_section_count; i++) {
        cJSON *sec = cJSON_CreateObject();
        cJSON_AddNumberToObject(sec, "shape_id", (double)g_device_config.tank_sections[i].shape);
        cJSON_AddNumberToObject(sec, "length_m",   g_device_config.tank_sections[i].params.length_m);
        cJSON_AddNumberToObject(sec, "width_m",    g_device_config.tank_sections[i].params.width_m);
        cJSON_AddNumberToObject(sec, "height_m",   g_device_config.tank_sections[i].params.height_m);
        cJSON_AddNumberToObject(sec, "radius_m",   g_device_config.tank_sections[i].params.radius_m);
        cJSON_AddNumberToObject(sec, "radius_b_m", g_device_config.tank_sections[i].params.radius_b_m);
        cJSON_AddItemToArray(secs, sec);
    }
    cJSON_AddItemToObject(root, "tank_sections", secs);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return ESP_ERR_NO_MEM;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_str(h, NVS_KEY, json_str);
        if (err == ESP_OK) nvs_commit(h);
        nvs_close(h);
    }
    free(json_str);

    if (err == ESP_OK) {
        ESP_LOGI(TAG_CFG, "Config saved to NVS");
    } else {
        ESP_LOGE(TAG_CFG, "NVS save failed: %s", esp_err_to_name(err));
    }
    return err;
}

/* ── Serialize current config to JSON string ────────────────────────────── */
/* Returns a heap-allocated JSON string — caller must free() it.             */
/* Returns NULL on allocation failure.                                       */
char *device_config_to_json_str(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "reporting_interval_s", g_device_config.reporting_interval_s);
    cJSON_AddNumberToObject(root, "timezone_offset_min",  g_device_config.timezone_offset_min);
    cJSON_AddBoolToObject  (root, "gps_enabled",          g_device_config.gps_enabled);
    cJSON_AddBoolToObject  (root, "debug_mode",           g_device_config.debug_mode);
    cJSON_AddStringToObject(root, "tank_shape",           g_device_config.tank_shape_str);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "length_m",   g_device_config.tank_params.length_m);
    cJSON_AddNumberToObject(p, "width_m",    g_device_config.tank_params.width_m);
    cJSON_AddNumberToObject(p, "height_m",   g_device_config.tank_params.height_m);
    cJSON_AddNumberToObject(p, "radius_m",   g_device_config.tank_params.radius_m);
    cJSON_AddNumberToObject(p, "radius_b_m", g_device_config.tank_params.radius_b_m);
    cJSON_AddItemToObject(root, "tank_shape_params", p);

    cJSON *secs = cJSON_CreateArray();
    for (int i = 0; i < g_device_config.tank_section_count; i++) {
        cJSON *sec = cJSON_CreateObject();
        cJSON_AddNumberToObject(sec, "shape_id",   (double)g_device_config.tank_sections[i].shape);
        cJSON_AddNumberToObject(sec, "length_m",   g_device_config.tank_sections[i].params.length_m);
        cJSON_AddNumberToObject(sec, "width_m",    g_device_config.tank_sections[i].params.width_m);
        cJSON_AddNumberToObject(sec, "height_m",   g_device_config.tank_sections[i].params.height_m);
        cJSON_AddNumberToObject(sec, "radius_m",   g_device_config.tank_sections[i].params.radius_m);
        cJSON_AddNumberToObject(sec, "radius_b_m", g_device_config.tank_sections[i].params.radius_b_m);
        cJSON_AddItemToArray(secs, sec);
    }
    cJSON_AddItemToObject(root, "tank_sections", secs);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str; /* caller must free() */
}

/* ── NVS load ───────────────────────────────────────────────────────────── */
esp_err_t device_config_load_from_nvs(void) {
    device_config_load_defaults();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_CFG, "No saved config in NVS — using defaults");
        return err;
    }

    size_t len = NVS_MAX_LEN;
    char *buf = malloc(len);
    if (!buf) { nvs_close(h); return ESP_ERR_NO_MEM; }

    err = nvs_get_str(h, NVS_KEY, buf, &len);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGW(TAG_CFG, "Config key not found — using defaults");
        free(buf);
        return err;
    }

    /* Parse without saving again (already in NVS) */
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG_CFG, "Corrupt NVS config — using defaults");
        return ESP_FAIL;
    }

    cJSON *it;
    if ((it = cJSON_GetObjectItem(root, "reporting_interval_s")) && cJSON_IsNumber(it))
        g_device_config.reporting_interval_s = (uint32_t)it->valuedouble;
    if ((it = cJSON_GetObjectItem(root, "timezone_offset_min"))  && cJSON_IsNumber(it))
        g_device_config.timezone_offset_min = (int)it->valuedouble;
    if ((it = cJSON_GetObjectItem(root, "gps_enabled"))          && cJSON_IsBool(it))
        g_device_config.gps_enabled = cJSON_IsTrue(it);
    if ((it = cJSON_GetObjectItem(root, "debug_mode"))           && cJSON_IsBool(it))
        g_device_config.debug_mode = cJSON_IsTrue(it);
    if ((it = cJSON_GetObjectItem(root, "tank_shape"))           && cJSON_IsString(it)) {
        strncpy(g_device_config.tank_shape_str, it->valuestring, TANK_SHAPE_MAX_LEN - 1);
        g_device_config.tank_shape = shape_from_str(it->valuestring);
    }
    parse_shape_params(cJSON_GetObjectItem(root, "tank_shape_params"), &g_device_config.tank_params);

    cJSON *secs = cJSON_GetObjectItem(root, "tank_sections");
    if (secs && cJSON_IsArray(secs)) {
        int count = cJSON_GetArraySize(secs);
        if (count > TANK_SECTIONS_MAX) count = TANK_SECTIONS_MAX;
        g_device_config.tank_section_count = 0;
        for (int i = 0; i < count; i++) {
            cJSON *sec = cJSON_GetArrayItem(secs, i);
            if (!sec) continue;
            tank_section_t *s = &g_device_config.tank_sections[g_device_config.tank_section_count];
            cJSON *sid = cJSON_GetObjectItem(sec, "shape_id");
            if (sid && cJSON_IsNumber(sid)) s->shape = (tank_shape_e)(int)sid->valuedouble;
            parse_shape_params(sec, &s->params);
            g_device_config.tank_section_count++;
        }
    }
    cJSON_Delete(root);

    /* Apply timezone from loaded config */
    set_esp32_timezone(g_device_config.timezone_offset_min / 60);

    ESP_LOGI(TAG_CFG, "Config loaded from NVS: interval=%lus tz=%dmin shape=%s h=%.2fm",
             (unsigned long)g_device_config.reporting_interval_s,
             g_device_config.timezone_offset_min,
             g_device_config.tank_shape_str,
             g_device_config.tank_params.height_m);
    return ESP_OK;
}

/* ── Apply from MQTT JSON ───────────────────────────────────────────────── */
esp_err_t device_config_apply_from_json(const char *json_str) {
    if (!json_str) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG_CFG, "Failed to parse config JSON");
        return ESP_FAIL;
    }

    cJSON *it;

    if ((it = cJSON_GetObjectItem(root, "reporting_interval_s")) && cJSON_IsNumber(it)) {
        int v = (int)it->valuedouble;
        if (v >= 5 && v <= 86400)
            g_device_config.reporting_interval_s = (uint32_t)v;
    }

    if ((it = cJSON_GetObjectItem(root, "timezone_offset_min")) && cJSON_IsNumber(it)) {
        g_device_config.timezone_offset_min = (int)it->valuedouble;
        /* Convert minutes → hours for the system call */
        int tz_h = g_device_config.timezone_offset_min / 60;
        set_esp32_timezone(tz_h);
        ESP_LOGI(TAG_CFG, "Timezone updated: %d min (%+d h)", g_device_config.timezone_offset_min, tz_h);
    }

    if ((it = cJSON_GetObjectItem(root, "gps_enabled")) && cJSON_IsBool(it))
        g_device_config.gps_enabled = cJSON_IsTrue(it);

    if ((it = cJSON_GetObjectItem(root, "debug_mode")) && cJSON_IsBool(it))
        g_device_config.debug_mode = cJSON_IsTrue(it);

    if ((it = cJSON_GetObjectItem(root, "tank_shape")) && cJSON_IsString(it)) {
        strncpy(g_device_config.tank_shape_str, it->valuestring, TANK_SHAPE_MAX_LEN - 1);
        g_device_config.tank_shape_str[TANK_SHAPE_MAX_LEN - 1] = '\0';
        g_device_config.tank_shape = shape_from_str(it->valuestring);
    }

    parse_shape_params(cJSON_GetObjectItem(root, "tank_shape_params"), &g_device_config.tank_params);

    cJSON *secs = cJSON_GetObjectItem(root, "tank_sections");
    if (secs && cJSON_IsArray(secs)) {
        int count = cJSON_GetArraySize(secs);
        if (count > TANK_SECTIONS_MAX) count = TANK_SECTIONS_MAX;
        g_device_config.tank_section_count = 0;
        for (int i = 0; i < count; i++) {
            cJSON *sec = cJSON_GetArrayItem(secs, i);
            if (!sec) continue;
            tank_section_t *s = &g_device_config.tank_sections[g_device_config.tank_section_count];
            cJSON *shape_it = cJSON_GetObjectItem(sec, "shape");
            if (shape_it && cJSON_IsString(shape_it))
                s->shape = shape_from_str(shape_it->valuestring);
            parse_shape_params(cJSON_GetObjectItem(sec, "params"), &s->params);
            g_device_config.tank_section_count++;
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG_CFG, "Config applied: interval=%lus tz=%dmin gps=%d debug=%d shape=%s h=%.2fm",
             (unsigned long)g_device_config.reporting_interval_s,
             g_device_config.timezone_offset_min,
             g_device_config.gps_enabled,
             g_device_config.debug_mode,
             g_device_config.tank_shape_str,
             g_device_config.tank_params.height_m);

    device_config_save_to_nvs();
    return ESP_OK;
}

/* ── Tank geometry ──────────────────────────────────────────────────────── */
float device_config_get_tank_height_cm(void) {
    const tank_shape_params_t *p = &g_device_config.tank_params;
    switch (g_device_config.tank_shape) {
        case TANK_RECTANGULAR:
        case TANK_CYLINDER_VERTICAL:
        case TANK_CONE_VERTICAL:
        case TANK_ELLIPSE_VERTICAL:
            return p->height_m * 100.0f;
        case TANK_CYLINDER_HORIZONTAL:
        case TANK_SPHERE:
            return p->radius_m * 2.0f * 100.0f;
        case TANK_CAPSULE:
            return (p->length_m + 2.0f * p->radius_m) * 100.0f;
        case TANK_MULTI_SECTION: {
            float max_h = 0.0f;
            for (int i = 0; i < g_device_config.tank_section_count; i++) {
                float h = g_device_config.tank_sections[i].params.height_m;
                if (h > max_h) max_h = h;
            }
            return (max_h > 0.0f ? max_h : p->height_m) * 100.0f;
        }
        default:
            return p->height_m * 100.0f;
    }
}

static float calc_section_volume_l(tank_shape_e shape, const tank_shape_params_t *p, float f) {
    if (f <= 0.0f) return 0.0f;
    if (f >  1.0f) f = 1.0f;
    const float PI = 3.14159265f;

    switch (shape) {
        case TANK_RECTANGULAR:
            return p->length_m * p->width_m * (f * p->height_m) * 1000.0f;

        case TANK_CYLINDER_VERTICAL:
            return PI * p->radius_m * p->radius_m * (f * p->height_m) * 1000.0f;

        case TANK_CYLINDER_HORIZONTAL: {
            float r = p->radius_m;
            float h = f * 2.0f * r;
            if (h > 2.0f * r) h = 2.0f * r;
            float area = r * r * acosf((r - h) / r) - (r - h) * sqrtf(2.0f * r * h - h * h);
            return area * p->length_m * 1000.0f;
        }

        case TANK_CONE_VERTICAL:
            /* vertex-down cone: V = (1/3)π r² h × f³ */
            return (1.0f/3.0f) * PI * p->radius_m * p->radius_m
                   * p->height_m * f * f * f * 1000.0f;

        case TANK_ELLIPSE_VERTICAL:
            return PI * p->radius_m * p->radius_b_m * (f * p->height_m) * 1000.0f;

        case TANK_SPHERE: {
            float r = p->radius_m;
            float h = f * 2.0f * r;
            return PI * h * h * (3.0f * r - h) / 3.0f * 1000.0f;
        }

        case TANK_CAPSULE: {
            float r = p->radius_m;
            float L = p->length_m;
            float total_h = 2.0f * r + L;
            float h = f * total_h;
            float vol;
            if (h <= r) {
                vol = PI * h * h * (3.0f * r - h) / 3.0f;
            } else if (h <= r + L) {
                vol = (2.0f/3.0f) * PI * r * r * r + PI * r * r * (h - r);
            } else {
                float top_h = h - (r + L);
                vol = (2.0f/3.0f) * PI * r * r * r
                    + PI * r * r * L
                    + PI * top_h * top_h * (3.0f * r - top_h) / 3.0f;
            }
            return vol * 1000.0f;
        }

        default:
            return 0.0f;
    }
}

float device_config_calc_volume_l(float level_cm) {
    float total_height_cm = device_config_get_tank_height_cm()+2.5;
    float f;
    
    if (total_height_cm <= 0.0f) {
        f = 0.0f;
    } else {
        f = level_cm / total_height_cm;
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
    }
    if (g_device_config.tank_shape == TANK_MULTI_SECTION) {
        float total = 0.0f;
        for (int i = 0; i < g_device_config.tank_section_count; i++)
            total += calc_section_volume_l(g_device_config.tank_sections[i].shape,
                                           &g_device_config.tank_sections[i].params, f);
        return total;
    }
    return calc_section_volume_l(g_device_config.tank_shape, &g_device_config.tank_params, f);
}

float device_config_calc_capacity_l(void) {
    float offset_level_mm =25;
    float max_height_mm=(device_config_get_tank_height_cm()*10)-offset_level_mm;
    return device_config_calc_volume_l(max_height_mm );
}
