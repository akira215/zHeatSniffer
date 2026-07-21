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
    
    _ahd = new DriverAhd();
    // Setup UART1

      uart_config_t uart_config = {
        .baud_rate = UART0_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,  // Use APB clock for ESP32-C6
    };

    //_rts.off(); // Set RTS low to put MAX485 in receive mode (if used with MAX485)

    // Configure UART
    uart_driver_install(UART0_NUM, UART0_BUFFER_SIZE, UART0_BUFFER_SIZE, 0, NULL, 0);
    uart_param_config(UART0_NUM, &uart_config);
    uart_set_pin(UART0_NUM, UART0_TX_PIN, UART0_RX_PIN, UART0_RTS_PIN, UART0_CTS_PIN);
    ESP_LOGI(TAG, "UART0 initialized at %d baud", UART0_BAUD_RATE);
    

    _ahd->start();

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


