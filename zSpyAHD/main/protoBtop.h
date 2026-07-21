#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

void app_main(void)
{
    // Clear screen and home cursor once at startup
    printf("\e[2J\e[H");

    while (1) {
        // Move cursor to top-left corner without clearing
        printf("\e[H");

        // Draw the static interface layout
        printf("========================================\n");
        printf("       ESP32 SYSTEM MONITOR (BTOP)      \n");
        printf("========================================\n");
        
        // Print dynamic data in fixed rows
        printf("Uptime    : %lld seconds\n", esp_timer_get_time() / 1000000);
        printf("Free Heap : %lu bytes\n", (unsigned long)esp_get_free_heap_size());
        printf("Minimum   : %lu bytes\n", (unsigned long)esp_get_minimum_free_heap_size());
        
        printf("========================================\n");
        printf("Press Ctrl+C in your monitor to exit...\n");

        // Wait 1 second before rewriting over the same text
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}