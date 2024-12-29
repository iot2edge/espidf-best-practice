#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

#define BUTTON_TASK_STACK_SIZE 2048
#define BUTTON_TASK_PRIORITY   5
#define MAX_BUTTONS           5
#define BUTTON_EVENT_QUEUE_SIZE 10

/**
 * @brief Button handling method
 */
typedef enum {
    BUTTON_HANDLER_POLLING,    // Uses polling method
    BUTTON_HANDLER_ISR        // Uses interrupt method
} button_handler_method_t;

/**
 * @brief Button events enumeration
 */
typedef enum {
    BUTTON_PRESS,
    BUTTON_RELEASE,
    BUTTON_LONG_PRESS
} button_event_t;

/**
 * @brief Button callback function type
 */
typedef void (*button_callback_fn)(button_event_t event, void* user_data);

/**
 * @brief Button configuration structure
 */
typedef struct {
    gpio_num_t gpio_num;           // GPIO number for button
    uint32_t debounce_ms;         // Debounce time in milliseconds
    uint32_t long_press_ms;       // Time in ms to trigger long press
    bool pull_up;                 // True for pull-up, false for pull-down
    button_callback_fn callback;   // Callback function
    void* user_data;              // User data passed to callback
    TaskHandle_t task_handle;     // FreeRTOS task handle (internal use)
    bool active_low;              // True if button is active low
    button_handler_method_t method; // Handling method (polling or ISR)
} button_config_t;

/**
 * @brief Initialize button handler system
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_handler_init(void);

/**
 * @brief start monitoring a button with given configuration
 * @param config button configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_handler_start(button_config_t* config);

/**
 * @brief stop monitoring a button with given configuration
 * @param config button configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t button_handler_stop(button_config_t* config);

/**
 * @brief Deiniitialize button handler system
 * @return ESP_OK on success, error code otherwise
 * */
esp_err_t button_handler_deinit(void);

#endif // BUTTON_HANDLER_H