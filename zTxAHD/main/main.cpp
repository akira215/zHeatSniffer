//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "main.h"
#include "driver/uart.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <functional>

#include <iostream>

#include <vector> // todel
#include "esp_random.h"
#include "esp_spiffs.h"


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

// Task to handle receiving data on UART1
static void uart1_rx_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *) malloc(UART0_BUFFER_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART1 RX Task started. Listening on GPIO %d...", UART1_RX_PIN);

    while (1) {
        // Read data from UART1 buffer (Non-blocking look with a 10ms timeout)
        int len = uart_read_bytes(UART1_NUM, data, UART0_BUFFER_SIZE - 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            data[len] = '\0'; // Null-terminate string payload if applicable
            
            // Print raw bytes to the USB console log
            ESP_LOGI(TAG, "UART1 RECEIVED %d bytes:", len);
            ESP_LOG_BUFFER_HEX(TAG, data, len);
        }
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    free(data);
    vTaskDelete(NULL);
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

void Main::init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL, // Uses the default "spiffs" partition label
      .max_files = 5,          // Maximum number of files that can be open at the same time
      .format_if_mount_failed = true // Formats the partition if it is empty/corrupted
    };

    // Use settings to initialize and mount SPIFFS filesystem
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition in partition table");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Verify storage size information
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}


void Main::setup(void)
{
    ESP_LOGI(TAG,"---------- Setup ----------");

    init_spiffs();  // init the VFS (SPI Flash File System)


    //_neoLed.set_pixel(1, 1, 1); // blue
    //_neoLed.send();
    

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

    //uart_driver_install(UART1_NUM, UART0_BUFFER_SIZE, UART0_BUFFER_SIZE, 0, NULL, 0);
    //uart_param_config(UART1_NUM, &uart_config);
    //uart_set_pin(UART1_NUM, UART1_TX_PIN, UART1_RX_PIN, UART0_RTS_PIN, UART0_CTS_PIN);

    //xTaskCreate(uart1_rx_task, "uart1_rx_task", 4096, NULL, 5, NULL);

    //ESP_LOGI(TAG, "UART1 initialized and task created");

    vTaskDelay(pdMS_TO_TICKS(2000));

    //_aldes->start(CONFIG_VMC_POLLING_MS, CONFIG_VMC_POLLING_GEN_DATA);

}

// Task to read file and stream over UART
void Main::uart_file_sender_task(const char *pvParameters) {
    const char *file_path = (const char *)pvParameters;
    char line_buffer[LINE_BUF_SIZE];
    uint8_t tx_packet_buffer[LINE_BUF_SIZE / 2]; // Holds converted binary bytes for transmission

    while (1) {
        FILE *f = fopen(file_path, "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file: %s", file_path);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        ESP_LOGI(TAG, "Starting multi-byte hex file transmission: %s", file_path);

        // Read line by line until End Of File (EOF)
        while (fgets(line_buffer, sizeof(line_buffer), f) != NULL) {
            int byte_count = 0;
            char *saveptr;
            
            // Tokenize the line using spaces, carriage returns, and newlines as delimiters
            char *token = strtok_r(line_buffer, " \r\n", &saveptr);
            
            while (token != NULL) {
                unsigned int hex_val = 0;
                
                // Parse the individual token (e.g., "3A" or "0d") into a hex integer
                if (sscanf(token, "%2x", &hex_val) == 1) {
                    tx_packet_buffer[byte_count] = (uint8_t)hex_val;
                    byte_count++;
                }
                
                // Move to the next space-separated hex byte on the same line
                token = strtok_r(NULL, " \r\n", &saveptr);
            }

            // If we extracted valid bytes, transmit the entire sequence at once
            if (byte_count > 0) {
                int bytes_sent = uart_write_bytes(UART0_NUM, tx_packet_buffer, byte_count);
                ESP_LOG_BUFFER_HEX(TAG, tx_packet_buffer, byte_count);
                if (bytes_sent < 0) {
                    ESP_LOGE(TAG, "UART TX error");
                } else {
                    ESP_LOGI(TAG, "Sent line packet: %d raw bytes", bytes_sent);
                }
            }

            // Random delay *after* sending the complete line
            uint32_t random_delay = MIN_WAIT_MS + (esp_random() % (MAX_WAIT_MS - MIN_WAIT_MS + 1));
            vTaskDelay(pdMS_TO_TICKS(random_delay));
        }

        ESP_LOGI(TAG, "Reached EOF. Rewinding to start...");
        fclose(f);
    }
}

/**
 * Read data from UART1 (blocking - waits up to timeout_ms)
 * Returns: number of bytes read, or -1 on error
 */
int Main::uart1_read(uint8_t *data, int max_len, int timeout_ms) {
    int len = uart_read_bytes(UART0_NUM, data, max_len, pdMS_TO_TICKS(timeout_ms));
    return len;
}

/**
 * Write data to UART1
 * Returns: number of bytes written
 */
int Main::uart1_write(const uint8_t *data, int len) {
    return uart_write_bytes(UART0_NUM, (const char *)data, len);
}

/**
 * Read available bytes without blocking
 */
int Main::uart1_read_available(uint8_t *data, int max_len) {
    size_t available = 0;
    uart_get_buffered_data_len(UART0_NUM, &available);
    
    if (available > 0) {
        int to_read = (available < max_len) ? available : max_len;
        return uart_read_bytes(UART0_NUM, data, to_read, 0);
    }
    return 0;
}



void Main::run(void)
{
    uart_file_sender_task("/spiffs/data.txt");



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


