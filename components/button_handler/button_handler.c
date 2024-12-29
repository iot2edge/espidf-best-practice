#include "button_handler.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_DEBUG
#endif

static const char *TAG = "button_handler";

// Mutex for thread-safe button operations
static SemaphoreHandle_t button_mutex = NULL;

// Queue for ISR events
static QueueHandle_t gpio_evt_queue = NULL;

// Array to keep track of active buttons
static button_config_t* active_buttons[MAX_BUTTONS] = {0};

// Structure for ISR events
typedef struct {
    gpio_num_t gpio_num;
    uint64_t timestamp;
} gpio_isr_evt_t;

// ISR handler
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    gpio_num_t gpio_num = (gpio_num_t)arg;
    gpio_isr_evt_t evt = {
        .gpio_num = gpio_num,
        .timestamp = esp_timer_get_time()
    };
    xQueueSendFromISR(gpio_evt_queue, &evt, NULL);
}

// Configure GPIO for button
static esp_err_t configure_button_gpio(const button_config_t* config) {
    gpio_config_t io_conf = {
        .intr_type = (config->method == BUTTON_HANDLER_ISR) ? GPIO_INTR_ANYEDGE : GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << config->gpio_num),
        .pull_up_en = config->pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->pull_up ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE
    };
    return gpio_config(&io_conf);
}

// Task to handle ISR events
static void gpio_isr_task(void* arg) {
    gpio_isr_evt_t evt;
    static uint64_t last_isr_time[MAX_BUTTONS] = {0};
    static bool is_pressed[MAX_BUTTONS] = {0};
    static uint64_t press_start_time[MAX_BUTTONS] = {0};
    static bool long_press_triggered[MAX_BUTTONS] = {0};

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY)) {
            button_config_t* button = NULL;
            int button_index = -1;

            // Find corresponding button config
            for (int i = 0; i < MAX_BUTTONS; i++) {
                if (active_buttons[i] && active_buttons[i]->gpio_num == evt.gpio_num) {
                    button = active_buttons[i];
                    button_index = i;
                    break;
                }
            }

            if (button && button_index >= 0) {
                uint64_t current_time = evt.timestamp;

                // Debounce check
                if ((current_time - last_isr_time[button_index]) >= (button->debounce_ms * 1000)) {
                    int level = gpio_get_level(evt.gpio_num);
                    bool button_pressed = button->active_low ? (level == 0) : (level == 1);

                    if (button_pressed != is_pressed[button_index]) {
                        is_pressed[button_index] = button_pressed;

                        if (button_pressed) {
                            // Button press
                            press_start_time[button_index] = current_time;
                            long_press_triggered[button_index] = false;
                            if (button->callback) {
                                button->callback(BUTTON_PRESS, button->user_data);
                            }
                        } else {
                            // Button release
                            if (button->callback && !long_press_triggered[button_index]) {
                                button->callback(BUTTON_RELEASE, button->user_data);
                            }
                        }
                    }

                    // Check for long press
                    if (is_pressed[button_index] && !long_press_triggered[button_index]) {
                        if ((current_time - press_start_time[button_index]) >= (button->long_press_ms * 1000)) {
                            if (button->callback) {
                                button->callback(BUTTON_LONG_PRESS, button->user_data);
                            }
                            long_press_triggered[button_index] = true;
                        }
                    }

                    last_isr_time[button_index] = current_time;
                }
            }
        }
    }
}

// Initialize button handler system
esp_err_t button_handler_init(void) {
    if (button_mutex == NULL) {
        button_mutex = xSemaphoreCreateMutex();
        if (button_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create button mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (gpio_evt_queue == NULL) {
        gpio_evt_queue = xQueueCreate(BUTTON_EVENT_QUEUE_SIZE, sizeof(gpio_isr_evt_t));
        if (gpio_evt_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create GPIO event queue");
            vSemaphoreDelete(button_mutex);
            button_mutex = NULL;
            return ESP_ERR_NO_MEM;
        }

        // Create ISR handling task
        xTaskCreate(gpio_isr_task, "gpio_isr_task", 2048, NULL, 10, NULL);

        // Install GPIO ISR service
        esp_err_t ret = gpio_install_isr_service(0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service");
            vQueueDelete(gpio_evt_queue);
            gpio_evt_queue = NULL;
            vSemaphoreDelete(button_mutex);
            button_mutex = NULL;
            return ret;
        }
    }

    return ESP_OK;
}

// Start monitoring a button with given configuration
esp_err_t button_handler_start(button_config_t* config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (button_mutex == NULL || (config->method == BUTTON_HANDLER_ISR && gpio_evt_queue == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    if (xSemaphoreTake(button_mutex, portMAX_DELAY) == pdTRUE) {
        // Find free slot
        int slot = -1;
        for (int i = 0; i < MAX_BUTTONS; i++) {
            if (active_buttons[i] == NULL) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {
            ESP_LOGE(TAG, "Maximum number of buttons reached");
            xSemaphoreGive(button_mutex);
            return ESP_ERR_NO_MEM;
        }

        // Configure GPIO
        ret = configure_button_gpio(config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO");
            xSemaphoreGive(button_mutex);
            return ret;
        }

        if (config->method == BUTTON_HANDLER_ISR) {
            // Add ISR handler
            ret = gpio_isr_handler_add(config->gpio_num, gpio_isr_handler, (void*)config->gpio_num);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add ISR handler");
                xSemaphoreGive(button_mutex);
                return ret;
            }
        } else {
            // Create polling task
        }

        active_buttons[slot] = config;
        xSemaphoreGive(button_mutex);
    }

    return ret;
}

// Stop monitoring a specific button
esp_err_t button_handler_stop(button_config_t* config) {
    if (button_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_ERR_NOT_FOUND;

    if (xSemaphoreTake(button_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_BUTTONS; i++) {
            if (active_buttons[i] && active_buttons[i]->gpio_num == config->gpio_num) {
                if (active_buttons[i]->method == BUTTON_HANDLER_ISR) {
                    gpio_isr_handler_remove(config->gpio_num);
                } else if (active_buttons[i]->task_handle != NULL) {
                    vTaskDelete(active_buttons[i]->task_handle);
                }
                active_buttons[i] = NULL;
                ret = ESP_OK;
                break;
            }
        }
        xSemaphoreGive(button_mutex);
    }

    return ret;
}

// Deinitialize button handler system
esp_err_t button_handler_deinit(void) {
    if (button_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(button_mutex, portMAX_DELAY) == pdTRUE) {
        // Stop all button monitoring
        for (int i = 0; i < MAX_BUTTONS; i++) {
            if (active_buttons[i]) {
                if (active_buttons[i]->method == BUTTON_HANDLER_ISR) {
                    gpio_isr_handler_remove(active_buttons[i]->gpio_num);
                } else if (active_buttons[i]->task_handle != NULL) {
                    vTaskDelete(active_buttons[i]->task_handle);
                }
                active_buttons[i] = NULL;
            }
        }

        // Uninstall GPIO ISR service
        gpio_uninstall_isr_service();

        // Delete queue
        if (gpio_evt_queue) {
            vQueueDelete(gpio_evt_queue);
            gpio_evt_queue = NULL;
        }

        // Delete mutex
        vSemaphoreDelete(button_mutex);
        button_mutex = NULL;

        xSemaphoreGive(button_mutex);
    }

    return ESP_OK;
}
