#pragma once

#include "driver/uart.h"
#include "driver/gpio.h"
#include <vector>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // Required for Semaphore/Mutex APIs

#define UART1_NUM           UART_NUM_1

#define UART1_TX_PIN        GPIO_NUM_18 // Unused in this unidirectional test
#define UART1_RX_PIN        GPIO_NUM_19

#define UART1_BAUD_RATE     9600
#define UART1_BUFFER_SIZE   1024

#define START_BYTE          0x3A
#define STOP_BYTE1          0x0A
#define STOP_BYTE2          0x0D

#define MODBUS_FUNC_READ    0x03
#define MODBUS_FUNC_WRITE   0x10

#define REG_SLAVE_POINTER   0x0001

#define DISPLAY_N_PER_LINE  4 // number of register to be displayed on one line

#define REGISTER_MAP_SIZE 100 // 100 uint16 register address could be stored to increase in case of more

#define RED   "\x1b[31m"
#define GREEN "\x1b[32m"
#define RESET "\x1b[0m"



class RegisterMap 
{
public:
    struct register_t {
        uint16_t address;
        uint16_t value;
        bool     updated;

        // Comparator needed for binary searching by address
        bool operator<(const register_t & other) const {
            return address < other.address;
        }
    };
private:
    std::vector<register_t> _registers;
    mutable SemaphoreHandle_t _mutex; // Mutable allows locking inside 'const' methods

public:
    // Constructor
    RegisterMap() {
        _mutex = xSemaphoreCreateMutex();
    }

    // Destructor
    ~RegisterMap() {
        if (_mutex != nullptr) {
            vSemaphoreDelete(_mutex);
        }
    }

    // Expose manual locking for safe raw reference/pointer manipulation
    void lock() const { 
        xSemaphoreTake(_mutex, portMAX_DELAY); 
    }
    
    void unlock() const { 
        xSemaphoreGive(_mutex); 
    }

    // Pre-allocate memory to completely avoid runtime heap allocation
    void reserve(size_t capacity) {

         _registers.reserve(capacity);

    }

    // Read/Write operator (Needs strict locking)
    register_t& operator[](uint16_t address) {
        register_t target{address, 0, true};
        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);

        if (it == _registers.end() || it->address != address) {
            it = _registers.insert(it, register_t{address, 0, true});
        }
        register_t& ref = (*it);

        return ref;
    }

    // Const read operator
    // WARNING: Call map.lock() before using this, and map.unlock() after you are done processing the reference.
    const register_t& operator[](uint16_t address) const {
        register_t target{address, 0, true};
        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);
        if (it != _registers.end() && it->address == address) {
            return (*it);
        }
        return (*it);
    }

    // Handles both inserting new registers and updating existing ones
    void upsert(uint16_t address, uint16_t value) {

        register_t target{address, 0, true};
        
        // O(log N) binary search to find the correct position
        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);

        if (it != _registers.end() && it->address == address) {
            // Case 1: Register already exists, update it in place
            if (it->value == value)
                it->updated = false;
            else
                it->updated = true;
            it->value = value;
        } else {
            // Case 2: Register does not exist, insert it while keeping the vector sorted
            _registers.insert(it, register_t{address, value, true});
        }

    }

    /**
     * Efficiently updates or inserts a consecutive block of registers.
     * @param start_address The address of the first register in the block
     * @param values Pointer to the array of incoming consecutive values
     * @param count Number of registers in the block
     */
    void insert_block(uint16_t start_address, const uint16_t* values, size_t count) {
        if (count == 0 || values == nullptr) return;

        // Ensure vector has enough total capacity to prevent hidden reallocations
        if (_registers.size() + count > _registers.capacity()) {
            _registers.reserve(_registers.size() + count + 10);
        }

        // 1. Find where the block should start in our sorted vector
        register_t target{start_address, 0, true};
        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);

        size_t value_idx = 0;

        // 2. Overwrite existing registers that match the incoming block
        while (it != _registers.end() && value_idx < count && it->address == (start_address + value_idx)) {
            if (it->value == values[value_idx])
                it->updated = false;
            else   
                it->updated = true;
            it->value = values[value_idx];
            static_cast<void>(++it);
            value_idx++;
        }

        // If the entire block updated existing elements, we are done
        if (value_idx >= count) return;

        // 3. For remaining new registers, check if they fall into a clean gaps or the end
        // Prepare a temporary batch array to insert everything remaining in one go
        std::vector<register_t> new_entries;
        new_entries.reserve(count - value_idx);

        for (size_t i = value_idx; i < count; ++i) {
            new_entries.push_back(register_t{static_cast<uint16_t>(start_address + i), values[i], true});
        }

        // 4. Perform a single batch insertion, which moves memory down exactly ONCE
        _registers.insert(it, new_entries.begin(), new_entries.end());

    }

    // WARNING: Caller must manually use map.lock() / map.unlock() around this call
    register_t* getItem(uint16_t address) {
        register_t target{address, 0, true};
        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);
        if (it != _registers.end() && it->address == address) {
            return &(*it);
        }
        return nullptr;
    }

    // O(log N) lookup. Returns true if found, sets the value reference
    uint16_t getValue(uint16_t address) const {
        uint16_t result = 0;
        register_t target{address, 0, true};

        auto it = std::lower_bound(_registers.begin(), _registers.end(), target);

        if (it != _registers.end() && it->address == address) {
            result = it->value;
        }

        return result;
    }

    // WARNING: Caller must manually use map.lock() / map.unlock() around this call
    const register_t& at(uint16_t n) const {
        return _registers.at(n);
    }

    void setUpdated(uint16_t n, bool updated) {
        _registers.at(n).updated = updated;

    }

    size_t size() const {
        return _registers.size();
    }
};

// Driver class used for testing only
class DriverAhd final
{
    

    //AHD is kind of Modbus Ascii, frame shall be converted to ASCII first
    std::vector<char>    _data;  // The raw data as read on UART
    std::vector<uint8_t> _bytes; // Data which is interpretation of the Ascii from Raw

    RegisterMap         _registerMap;

    std::string  _lastErr;

    uint32_t    _nFrame     = 0;
    uint8_t     _slaveId    = 0;
    uint8_t     _function   = 0;
    uint8_t     _nBytes     = 0;
    uint16_t _startingAdd   = 0;
    uint16_t _nRegisters    = 0;
    uint16_t _writeAdd      = 0;
    uint16_t _nWrite        = 0;

public:
    DriverAhd();
    ~DriverAhd();

    static void uartRxTask(void *pvParameters);
    static void displayTask(void *pvParameters);

    void start();

private:
    void processFrame();
    void printRawFrame();
    void buildFrame();
    void decodeFrame();
    bool checkSum();
    
    // Helper function to convert a single hex char to its 4-bit numeric value
    static uint8_t hex_to_val(char ch);
    static void printVectorInline(const std::vector<uint8_t>& bytes);
    void printDashboard();

}; //class DriverAdh