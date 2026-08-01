//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "main.h"
#include "driver/uart.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <functional>

#include <iostream>

#include <vector> // todel


static const char *TAG = "MainApp";

Main App;

Main::Main()
{
    // Setting the log level for each module
    esp_log_level_set("Main_app", ESP_LOG_VERBOSE);  // Put verbose to check available stack
}



void Main::setup(void)
{
    ESP_LOGI(TAG,"---------- Setup ----------");
    
    _hbs = new DriverHomebus();

    _hbs->start();

    vTaskDelay(pdMS_TO_TICKS(3000));


}



void Main::run(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

  /*  
    int len = uart1_read(buffer, UART0_BUFFER_SIZE, 1000);
        
    if (len > 0) {
        ESP_LOGI(TAG, "Read %d bytes:", len);
        ESP_LOG_BUFFER_HEX(TAG, buffer, len);
    }
    //ESP_LOGI(TAG, "Out");
*/



}


extern "C" void app_main(void)
{
    ESP_LOGI(TAG,"---------- Starting App ----------");
    App.setup();

    while (true)
    {
        App.run();
    }    

    //should not reach here
}


