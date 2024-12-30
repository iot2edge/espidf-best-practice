#include "spiffs_manager.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_DEBUG
#endif

static const char *TAG = "SPIFFS_MANAGER";

esp_err_t spiffs_mananger_init(const char* base_path, const char* partition_label) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = base_path,
        .partition_label = partition_label,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS mounted successfully: total: %d, used: %d", total, used);
    return ESP_OK;
}

esp_err_t spiffs_mananger_deinit(const char* partition_label) {
    esp_err_t ret = esp_vfs_spiffs_unregister(partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS unmounted successfully");
    return ESP_OK;
}

esp_err_t spiffs_mananger_write(const char* path, const char* data, size_t len) {
    FILE* file = fopen(path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing (%s)", path);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, file);
    fclose(file);

    if (written != len) {
        ESP_LOGE(TAG, "Failed to write to file (%s)", path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Wrote %d bytes to file (%s)", written, path);
    return ESP_OK;
}

ssize_t spiffs_mananger_read(const char* path, char* data, size_t len) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading (%s)", path);
        return ESP_FAIL;
    }

    size_t read = fread(data, 1, len, file);
    fclose(file);

    if (read < 0) {
        ESP_LOGE(TAG, "Failed to read from file (%s)", path);
        return -1;
    }
    
    ESP_LOGI(TAG, "Read %d bytes from file (%s)", read, path);
    return read;
}