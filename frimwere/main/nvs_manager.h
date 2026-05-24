#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H
#include "commun_lib.h"
#include "nvs_flash.h"
#include "nvs.h"

/* ── uint32 helpers (used for persisting counters like flow pulse total) ──── */

esp_err_t nvs_save_u32(const char *ns, const char *key, uint32_t value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t nvs_load_u32(const char *ns, const char *key, uint32_t *out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_u32(h, key, out);
    nvs_close(h);
    return err;
}


esp_err_t init_nvs(){

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    return ret;
}
char* get_saved_data_from_flash(const char* namespace, const char* key) {
          
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        printf("Error (%s) nvs_flash_init!\n", esp_err_to_name( ret));
        ret = nvs_flash_init();
    }
        nvs_handle_t nvs_handle;

    // Open NVS
    esp_err_t err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
         printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        // saved_data_in_flash(namespace,key, "") ;
        return NULL;
    }

    // Read data from NVS
    char* value = NULL;
    size_t required_size;

    err = nvs_get_str(nvs_handle, key, NULL, &required_size);
    if (err == ESP_OK) {
        value = malloc(required_size);
        if (value) {
            err = nvs_get_str(nvs_handle, key, value, &required_size);
            if (err == ESP_OK) {
                printf("Value for key '%s' in namespace '%s': %s\n", key, namespace, value);
            } else {
                printf("Error (%s) reading from NVS!\n", esp_err_to_name(err));
                free(value);
               return NULL;
            }
        } else {
            printf("Error allocating memory!\n");
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("Key '%s' not found in namespace '%s'!\n", key, namespace);
         return NULL;
    } else {
        printf("Error (%s) reading from NVS!\n", esp_err_to_name(err));
          return NULL;
    }

    // Close NVS
    nvs_close(nvs_handle);

    return value;
}
esp_err_t saved_data_in_flash(const char* namespace, const char* key, const char* value) {
      
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        printf("Error (%s) nvs_flash_init!\n", esp_err_to_name( ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("nvs_flash_init= (%s)\n", esp_err_to_name( ret));
    nvs_handle_t nvs_handle;

    // Open NVS
    esp_err_t err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    
    }

    // Write data to NVS
    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        printf("Error (%s) writing to NVS!\n", esp_err_to_name(err));
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) committing changes to NVS!\n", esp_err_to_name(err));
    }

    // Close NVS
    nvs_close(nvs_handle);
    return err;
}
#endif /* NVS_CONFIG_H */