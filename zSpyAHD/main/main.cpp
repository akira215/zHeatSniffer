//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "main.h"
#include "driver/uart.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <functional>

#include <iostream>

#include <vector> // todel



static const char *TAG = "Main_app";
BlinkTask* Main::_ledBlinking = nullptr;
//GpioOutput Main::_led { (gpio_num_t)CONFIG_PIN_LED }; //TODO led pin number in config file

//GpioOutput Main::_rts { (gpio_num_t)GPIO_NUM_19 }; // RTS pin for UART1

Main App;

Main::Main()
{
    // Setting the log level for each module
    esp_log_level_set("Main_app", ESP_LOG_VERBOSE);  // Put verbose to check available stack
}

// static
void Main::ledFlash(uint64_t speed)
{
    if(speed == 0) 
    {
        if(_ledBlinking){
        delete _ledBlinking;
        _ledBlinking = nullptr;
        }
        _led.off();
    } else if(speed == -1) {
        if(_ledBlinking){
        delete _ledBlinking;
        _ledBlinking = nullptr;
        }
        _led.on();
    } else {
        if(!_ledBlinking)
            _ledBlinking = new BlinkTask(_led, speed); // very short flash
        else
            _ledBlinking->setBlinkPeriod(speed);
    }

}

//Static
void Main::shortPressHandler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGV(TAG,"Short Press detected %ld -",id);

    //ESP_LOGI(TAG,"Short Press detected");

}

//Static
void Main::longPressHandler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{

    ESP_LOGV(TAG,"Long Press detected %ld -",id);
    //ESP_LOGV(TAG,"Long Press detected ");

}


void Main::setup(void)
{
    ESP_LOGI(TAG,"---------- Setup ----------");
    

    _button.enablePullup();
    _buttonTask = new ButtonTask (_button);
    _buttonTask->setShortPressHandler(&shortPressHandler);
    _buttonTask->setLongPressHandler(&longPressHandler,(void*)this);
    
   
    // switch off led on UART1 as it is not used
    //_tx1.on();
    //_rx1.enablePullup();
    //_rts1.off();

    //_aldes = new AldesDriver();

    // Setup UART1

      uart_config_t uart_config = {
        .baud_rate = UART1_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,  // Use APB clock for ESP32-C6
    };

    //_rts.off(); // Set RTS low to put MAX485 in receive mode (if used with MAX485)

    // Configure UART
    uart_driver_install(UART1_NUM, UART1_BUFFER_SIZE, UART1_BUFFER_SIZE, 0, NULL, 0);
    uart_param_config(UART1_NUM, &uart_config);
    
    // Set GPIO pins
    uart_set_pin(UART1_NUM, UART1_TX_PIN, UART1_RX_PIN, UART1_RTS_PIN, UART1_CTS_PIN);

    ESP_LOGI(TAG, "UART1 initialized at %d baud", UART1_BAUD_RATE);


    vTaskDelay(pdMS_TO_TICKS(7000));

    //_aldes->start(CONFIG_VMC_POLLING_MS, CONFIG_VMC_POLLING_GEN_DATA);

}

/**
 * Read data from UART1 (blocking - waits up to timeout_ms)
 * Returns: number of bytes read, or -1 on error
 */
int Main::uart1_read(uint8_t *data, int max_len, int timeout_ms) {
    int len = uart_read_bytes(UART1_NUM, data, max_len, pdMS_TO_TICKS(timeout_ms));
    return len;
}

/**
 * Write data to UART1
 * Returns: number of bytes written
 */
int Main::uart1_write(const uint8_t *data, int len) {
    return uart_write_bytes(UART1_NUM, (const char *)data, len);
}

/**
 * Read available bytes without blocking
 */
int Main::uart1_read_available(uint8_t *data, int max_len) {
    size_t available = 0;
    uart_get_buffered_data_len(UART1_NUM, &available);
    
    if (available > 0) {
        int to_read = (available < max_len) ? available : max_len;
        return uart_read_bytes(UART1_NUM, data, to_read, 0);
    }
    return 0;
}



void Main::run(void)
{


    int len = uart1_read(buffer, UART1_BUFFER_SIZE, 10);
        
    if (len > 0) {
        ESP_LOGI(TAG, "Read %d bytes:", len);
        ESP_LOG_BUFFER_HEX(TAG, buffer, len);
    }



/*
    int len = uart1_read_available(buffer, UART1_BUFFER_SIZE);
        
    if (len > 0) {
        //ESP_LOGI(TAG, "Read %d bytes:", len);
        ESP_LOG_BUFFER_HEX(TAG, buffer, len);
    }
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


