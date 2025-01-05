#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "button_handler.h"
#include "spiffs_manager.h"


#define BUTTON_GPIO GPIO_NUM_0 // Change this to your button GPIO number

// Callback function to handle button events
void button_event_handler(button_event_t event, void* user_data) {
    switch (event) {
        case BUTTON_PRESS:
            printf("Button pressed!\n");
            break;
        case BUTTON_RELEASE:
            printf("Button released!\n");
            break;
        case BUTTON_LONG_PRESS:
            printf("Button long pressed!\n");
            break;
        default:
            break;
    }
}

void app_main(void) {
    // Initialize the button handler
    esp_err_t ret = button_handler_init();
    if (ret != ESP_OK) {
        printf("Failed to initialize button handler: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = spiffs_manager_init("/spiffs", "storage");
    if (ret != ESP_OK) {
        ESP_LOGE("Main", "Fail to initialize SPIFFS");
        return;
    }

    const char *data = "Hello, SPIFFS!";
    spiffs_manager_write("/spiffs/hello.txt", data, strlen(data));
    
    char buffer[64];
    ssize_t read = spiffs_manager_read("/spiffs/data.txt", buffer, sizeof(buffer));
    if (read > 0) {
        printf("Read from SPIFFS: %s\n", buffer);
    } else {
        printf("Failed to read from SPIFFS\n");
    }

    spiffs_manager_deinit("storage");

    // Configure the button
    button_config_t button_config = {
        .gpio_num = BUTTON_GPIO,
        .debounce_ms = 50,
        .long_press_ms = 1000,
        .pull_up = true,
        .callback = button_event_handler,
        .user_data = NULL,
        .active_low = true,
        .method = BUTTON_HANDLER_ISR // Use ISR method
    };

    // Start monitoring the button
    ret = button_handler_start(&button_config);
    if (ret != ESP_OK) {
        printf("Failed to start button handler: %s\n", esp_err_to_name(ret));
        button_handler_deinit();
        return;
    }

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to prevent watchdog timeout
    }

    // Stop monitoring the button (cleanup)
    button_handler_stop(&button_config);
    button_handler_deinit();
}
