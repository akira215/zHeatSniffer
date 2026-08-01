
#include <esp_log.h>
#include <numeric> // Required for std::accumulate
#include <cmath>
#include "driverAhd.h"

static const char *TAG = "DriverAhd";

DriverAhd::DriverAhd()
{
    uart_config_t uart_config = {
        .baud_rate = UART1_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,  // Use APB clock for ESP32-C6
    };

    uart_driver_install(UART1_NUM, UART1_BUFFER_SIZE, UART1_BUFFER_SIZE, 0, NULL, 0);
    uart_param_config(UART1_NUM, &uart_config);
    uart_set_pin(UART1_NUM, UART1_TX_PIN, UART1_RX_PIN, 
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART1 initialized at %d baud", UART1_BAUD_RATE);

    //Reserve to avoid heap allocation
    _registerMap.reserve(REGISTER_MAP_SIZE);

}

DriverAhd::~DriverAhd()
{

}

void DriverAhd::start()
{
    xTaskCreate(DriverAhd::uartRxTask, "DriverAhd_Task", 4096, this, 5, NULL);
    //ESP_LOGI(TAG, "UART1 initialized at %d baud", UART1_BAUD_RATE);
    //xTaskCreate(DriverAhd::displayTask, "Display_Task", 4096, this, 2, NULL);
    // Clear screen and home cursor once at startup
    printf("\e[2J\e[H");
}

//Static
void DriverAhd::uartRxTask(void *pvParameters)
{
    DriverAhd* instance = static_cast<DriverAhd*>(pvParameters);
    
    uint8_t *rxBuffer = (uint8_t *) malloc(UART1_BUFFER_SIZE);
    if (rxBuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART1 RX Task started. Listening on GPIO %d...", UART1_RX_PIN);

    int rxId = 0;

    while (1) {
         
        // Read raw data fragments from UART
        int len = uart_read_bytes(UART1_NUM, rxBuffer,
                                sizeof(rxBuffer), pdMS_TO_TICKS(10));

        for (int i = 0; i < len; i++) {
            uint8_t curByte = rxBuffer[i];

            //ESP_LOGI(TAG, "position [%d] - current byte: %u", i, curByte);

            // 1. Detect start byte: Reset buffer to start a fresh frame
            if (curByte == START_BYTE) {
                rxId = 0; 
                instance->_data.clear();

            }

            // 2. Append byte if we have room
            instance->_data.push_back(static_cast<char>(curByte));
            rxId++;

            // 3. Detect stop bytes: Look back for 0x0D 0x0A
            if (rxId >= 3 && instance->_data[rxId - 2] == STOP_BYTE2 
                            && instance->_data[rxId - 1] == STOP_BYTE1) {
                // Check if frame actually started with the correct byte
                if (instance->_data.at(0) == START_BYTE) {
                    // Process valid complete frame
                    instance->processFrame();
                }
                
                // Clear index to wait for next start byte
                rxId = 0; 
            }
        }
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    free(rxBuffer);
    vTaskDelete(NULL);
}

// Static
uint8_t DriverAhd::hex_to_val(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return 0;
}

void DriverAhd::processFrame()
{
    //printRawFrame();
    buildFrame();
    if(!checkSum()){
        _lastErr = std::string("Checksum error in a frame           ");
        return;
    }

    // printVectorInline(_bytes);
    decodeFrame();
    printDashboard();
        
}

void DriverAhd::printRawFrame()
{
    //AHD is kind of Modbus Ascii, frame shall be converted to ASCII first
    ESP_LOGI(TAG, "Frame Converted to ASCII (%d bytes):", _data.size());
    ESP_LOG_BUFFER_HEX(TAG, _data.data(), _data.size() );
    ESP_LOG_BUFFER_CHAR(TAG, _data.data(), _data.size() );
}

void DriverAhd::buildFrame()
{
    // start and stops are not excluded and each pair of ascii
    // is interpreted as a byte

    _bytes.clear(); 
    // Loop through the vector in pairs, ignoring start and 2 stops
    for (size_t i = 1; i < _data.size() - 3; i += 2) {

        uint8_t high_nibble = hex_to_val(_data[i]);
        uint8_t low_nibble = hex_to_val(_data[i + 1]);
            
        // Combine the two 4-bit values into a single 8-bit byte
        uint8_t byte_val = (high_nibble << 4) | low_nibble;
        _bytes.push_back(byte_val);
    }

    _nFrame++;
}

bool DriverAhd::checkSum()
{
    uint32_t sum = 0;
    // 1. Guard against empty or 1-byte vectors
    if (_bytes.size() > 1) {
        // 2. Accumulate from the start up to (end - 1) the last byte is the chksum
        sum = std::accumulate(_bytes.begin(), _bytes.end() - 1, 0);
    }

    uint8_t check = (uint8_t)((-sum) & 0xFF); 
    if (check == _bytes.back())
        return true;
    
    return false;
}

void DriverAhd::printVectorInline(const std::vector<uint8_t>& bytes) {
    for (uint8_t byte : bytes) {
        // %02X prints 0-15 as '00'-'0F' in uppercase hex
        printf("%02X ", byte); 
    }
    printf("\n"); // Newline at the end
}

void DriverAhd::decodeFrame()
{
    _slaveId    = _bytes[0];  //First Byte is slave address
    _function   = _bytes[1];  //Modbus function

    if (_function == MODBUS_FUNC_READ) {// function read register
        if (_bytes.size() == 7) { // Master query
            _startingAdd = (static_cast<uint16_t>(_bytes[2]) << 8) | _bytes[3];
            _nRegisters = (static_cast<uint16_t>(_bytes[4]) << 8) | _bytes[5];
        } else { // Slave Response
            if (_startingAdd == REG_SLAVE_POINTER) { // Master query slave what to do next
                // Here slave answer 3 words : function - start - num of reg
                if (_bytes[4] == MODBUS_FUNC_WRITE) {  // Capture when slave ask a write
                    _writeAdd = (static_cast<uint16_t>(_bytes[5]) << 8) | _bytes[6];
                    _nWrite = (static_cast<uint16_t>(_bytes[7]) << 8) | _bytes[8];
                }
            } else { // Slave response data
                _nBytes = _bytes[2];
                if (_nBytes != (_bytes.size() - 4))
                    _lastErr = std::string("Slave Answer read - size not equal to nb Bytes");
                else {
                    for (uint8_t i = 0; i < _nBytes / 2; ++i) {
                        uint16_t value = (static_cast<uint16_t>(_bytes[3+i*2]) << 8) | _bytes[4+i*2];

                        _registerMap.lock();
                        RegisterMap::register_t* reg = _registerMap.getItem(_startingAdd + i);
                        if (reg == nullptr) {
                            _registerMap[_startingAdd + i].value = value;
                            _registerMap[_startingAdd + i].updated = true;

                        } else {
                            if (reg->value == value)
                                reg->updated = false;
                            else {
                                reg->updated = true;
                                reg->value = value;
                            }
                        }
                        _registerMap.unlock();

                    }
                }
            }
            _nRegisters = 0;
            _startingAdd = 0;
        }
    } else if (_function ==  MODBUS_FUNC_WRITE) { // function write register
        if (_bytes.size() == 7) { // Slave Response
        
        } else { // Master query
            uint16_t start = (static_cast<uint16_t>(_bytes[2]) << 8) | _bytes[3];
            uint16_t n = (static_cast<uint16_t>(_bytes[4]) << 8) | _bytes[5];
            _nBytes = _bytes[6];

            if (_nBytes != (_bytes.size() - 8))
                _lastErr = std::string("Master Write - size not equal to nb Bytes      ");
            else {
                 for (uint8_t i = 0; i < _nBytes / 2; ++i) {
                    uint16_t value = (static_cast<uint16_t>(_bytes[7+i*2]) << 8) | _bytes[8+i*2];
                    
                    _registerMap.lock();
                    RegisterMap::register_t* reg = _registerMap.getItem(start + i);
                    if (reg == nullptr) {
                        _registerMap[start + i].value = value;
                        _registerMap[start + i].updated = true;

                    } else {
                        if (reg->value == value)
                            reg->updated = false;
                        else {
                            reg->updated = true;
                            reg->value = value;
                        }
                    }
                    _registerMap.unlock();
                }
            }

        }
    }

}

//Static
void DriverAhd::displayTask(void *pvParameters)
{
    DriverAhd* instance = static_cast<DriverAhd*>(pvParameters);
    
    // Clear screen and home cursor once at startup
    printf("\e[2J\e[H");

    while (1) {
        
        instance->printDashboard();

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

void DriverAhd::printDashboard()
{
    // Move cursor to top-left corner without clearing
    printf("\e[H");

    // Draw the static interface layout
    printf("-------------------------------------------------------------------------\n");
    printf("|                          AHD20 Controller Table                       |\n");
    printf("-------------------------------------------------------------------------\n");
    
    // Print dynamic data in fixed rows
    printf("|    Slave ID  0x%02X     |    Num of frames " RED "%*ld" RESET "   |    Function 0x%02X   |\n", 
                            _slaveId, 5, _nFrame, _function);
    printf("| Address requested by slave  0x%04X   |   Number of registers 0x%04X   |\n",
        _writeAdd, _nWrite);
    printf("-------------------------------------------------------------------------\n");
    printf("| Last Error: "RED"%*s"RESET"|\n", 58, _lastErr.c_str());
    
    _registerMap.lock();
    for (uint8_t i = 0; i <  _registerMap.size(); ++i) {
        if ((i >0 )&&(i % DISPLAY_N_PER_LINE == 0)) // Go to new line if i is multiple of num per line
            printf("|\n");
        auto data = _registerMap.at(i);
        printf("| 0x%04X : ", data.address);
        
        if(data.updated)
            printf(RED);
        
        printf("%*d ", 6, data.value);

        if(data.updated)
            printf(RESET);
        
    }
    _registerMap.unlock();

}