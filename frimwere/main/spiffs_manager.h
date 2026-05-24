#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include "commun_lib.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "mbedtls/md5.h"

static const char *TAG_spiffs = "spiffs";

void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // Initialize SPIFFS
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_spiffs, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_spiffs, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG_spiffs, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Check for SPIFFS file system info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_spiffs, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG_spiffs, "Partition size: total: %d, used: %d", total, used);
    }
}


void save_data_to_spiffs(uint32_t data[], size_t length, char* path) {
    // Allocate memory for pathfile
    char *pathfile = malloc(30);
    if (pathfile == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to allocate memory for pathfile");
        return;
    }

    snprintf(pathfile, 30, "/spiffs/%s", path);
    printf("save : %s\n", pathfile);

    // Open file for writing
    FILE* f = fopen(pathfile, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to open file for writing");
        free(pathfile); // Free allocated memory before returning
        return;
    }

    // Write data to file
    size_t written = fwrite(data, sizeof(uint32_t), length, f);
    if (written != length) {
        ESP_LOGE(TAG_spiffs, "Failed to write data to file");
    } else {
        ESP_LOGI(TAG_spiffs, "Data written successfully");
    }

    // Close the file
    fclose(f);
    free(pathfile); // Free allocated memory after usage
}
void read_data_from_spiffs(uint32_t data[], size_t length, const char* path) {
        // Allocate memory for pathfile
    char *pathfile = malloc(30);
    if (pathfile == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to allocate memory for pathfile");
        return;
    }

    snprintf(pathfile, 30, "/spiffs/%s", path);
    printf("save : %s\n", pathfile);
    // Open file for reading
    FILE* f = fopen(pathfile, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to open file for reading");
        return;
    }

    // Read data from file
    size_t read = fread(data, sizeof(uint32_t), length, f);
    if (read > 0) {
        ESP_LOGI(TAG_spiffs, "Data read successfully");
    } else {
        ESP_LOGE(TAG_spiffs, "Failed to read data from file");
    }

    // Close the file
    fclose(f);
}
char * read_data(const char* file_path) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to open file for reading");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // Allocate memory for the buffer based on file size + 1 for null terminator
  // char *retrieved_data = (char*)realloc(NULL, (file_size+2) * sizeof(char));
    char *retrieved_data=(char *)calloc(file_size+2,sizeof(char));
    if (retrieved_data == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to allocate memory for file content");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(retrieved_data, sizeof(char), file_size, file);
    if (read_size != (size_t)file_size) {
        ESP_LOGE(TAG_spiffs, "Error reading file content");
        fclose(file);
        free(retrieved_data); // Free memory if read was unsuccessful
        return NULL;
    }

    fclose(file);

    return retrieved_data;
}
esp_err_t store_data(const char* file_path, const char* data) {

    FILE* file = fopen(file_path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG_spiffs, "Failed to open file for writing");
        return ESP_FAIL ;
    }

    fprintf(file, "%s", data);
    fclose(file);
   // freeallocate(data);
    ESP_LOGI(TAG_spiffs, "Data stored successfully");
    return ESP_OK;
}
void delete_files(const char* path) {
  
        char file_path[20];
        snprintf(file_path, sizeof(file_path), "/spiffs/%s", path);

        if (remove(file_path) == 0) {
            ESP_LOGI(TAG_spiffs, "File %s deleted successfully", file_path);
        } else {
            ESP_LOGE(TAG_spiffs, "Failed to delete file %s", file_path);
        }
    
}
#endif /* SPIFFS_MANAGER_H */