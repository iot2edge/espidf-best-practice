#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initialize the SPIFFS file system.
 * 
 * @param base_path The base path for the SPIFFS partition will be mounted
 * @param partition_label The label of the SPIFFS partition
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure
 */
esp_err_t spiffs_mananger_init(const char* base_path, const char* partition_label);

/**
 * @brief Dinitialize the SPIFFS file system.
 * 
 * @param partition_label The label of the SPIFFS partition to unmount
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure
 */
esp_err_t spiffs_mananger_deinit(const char* partition_label);

/**
 * @brief Write data to a file in SPIFFS
 * 
 * @param path The file path (e.g. "/spiffs/myfile.txt")
 * @param data The data to write
 * @param len The length of the data to write
 * @return ssize_t Returns the number of bytes written, or -1 on failure
 */
esp_err_t spiffs_mananger_write(const char* path, const char* data, size_t len);

/**
 * @brief Read data from a file in SPIFFS
 * 
 * @param path The file path (e.g. "/spiffs/myfile.txt")
 * @param buffer The buffer to store the read data
 * @param len The length of the data to read
 * @return ssize_t Returns the number of bytes read, or -1 on failure
 */
esp_err_t spiffs_mananger_read(const char* path, char* buffer, size_t len);

#endif // SPIFFS_MANAGER_H