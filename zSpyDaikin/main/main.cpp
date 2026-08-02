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
    esp_log_level_set("DriverHomebus", ESP_LOG_INFO);
    esp_log_level_set("DriverDaikin", ESP_LOG_VERBOSE);
}



void Main::setup(void)
{
    ESP_LOGI(TAG,"---------- Setup ----------");
    
    _hbs = new DriverHomebus();
    _daikin = new DriverDaikin();

    _hbs->registerCallback(&DriverDaikin::callback, _daikin);

    _hbs->start();

    vTaskDelay(pdMS_TO_TICKS(30000));

    //_hbs->stop();

}



void Main::run(void)
{
    //_hbs->run();
    vTaskDelay(pdMS_TO_TICKS(10)); // let time to reset watchdog
}


extern "C" void app_main(void)
{
    ESP_LOGI(TAG,"---------- Starting App ----------");
    App.setup();
/*
    while (true)
    {
        App.run();
    }    
*/
    //should not reach here
}


