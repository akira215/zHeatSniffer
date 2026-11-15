//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "main.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <functional>

#include <iostream>

#include <vector> // todel
#include "esp_random.h"
#include "esp_spiffs.h"


static const char *TAG = "MainApp";

Main App;

Main::Main()
{
    // Setting the log level for each module
    esp_log_level_set("Main_app", ESP_LOG_VERBOSE);  // Put verbose to check available stack
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

// Task to read file and stream over RMT
void Main::SendRmtData(const std::string& file) {
    
    char line_buffer[LINE_BUF_SIZE];
    std::vector<uint8_t> bytesTx;
    bytesTx.reserve(LINE_BUF_SIZE / 2); // Reserve space for efficiency


    while (1) {
        FILE *f = fopen(file.c_str(), "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file: %s", file.c_str());
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        ESP_LOGI(TAG, "Starting multi-byte hex file transmission: %s", file.c_str());

        // Read line by line until End Of File (EOF)
        while (fgets(line_buffer, sizeof(line_buffer), f) != NULL) {
            char *saveptr;
            
            // Tokenize the line using spaces, carriage returns, and newlines as delimiters
            char *token = strtok_r(line_buffer, " \r\n", &saveptr);
            
            while (token != NULL) {
                unsigned int hex_val = 0;
                
                // Parse the individual token (e.g., "3A" or "0d") into a hex integer
                if (sscanf(token, "%2x", &hex_val) == 1) {
                    bytesTx.push_back(static_cast<uint8_t>(hex_val));
                }
                
                // Move to the next space-separated hex byte on the same line
                token = strtok_r(NULL, " \r\n", &saveptr);
            }

            // If we extracted valid bytes, transmit the entire sequence at once
            if (!bytesTx.empty()) {
                _hbs->sendBytes(bytesTx);
                
                ESP_LOGI(TAG, "Sent line packet: %d raw bytes", bytesTx.size());
                bytesTx.clear();
            }

            // Random delay *after* sending the complete line
            uint32_t random_delay = MIN_WAIT_MS + (esp_random() % (MAX_WAIT_MS - MIN_WAIT_MS + 1));
            vTaskDelay(pdMS_TO_TICKS(random_delay));
        }

        ESP_LOGI(TAG, "Reached EOF. Rewinding to start...");
        fclose(f);
    }
}



void Main::setup(void)
{
    ESP_LOGI(TAG,"---------- Setup ----------");
    init_spiffs();  // init the VFS (SPI Flash File System)
    
    _hbs = new DriverHomebus();

    //_hbs->start(); //start reception

    vTaskDelay(pdMS_TO_TICKS(3000));


}


void Main::run(void)
{
    SendRmtData("/spiffs/data.txt");
}


extern "C" void app_main(void)
{
    ESP_LOGI(TAG,"---------- Starting App ----------");
    App.setup();

    //while (true)
    //{
        App.run();
    //}    

    //should not reach here
}


