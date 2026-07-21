#pragma once

#include "driver/uart.h"
#include "driver/gpio.h"
#include <vector>
#include <string>

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
    std::vector<register_t> m_registers;

public:
    // Pre-allocate memory to completely avoid runtime heap allocation
    void reserve(size_t capacity) {
        m_registers.reserve(capacity);
    }

    // Single item assignment/lookup
    uint16_t& operator[](uint16_t address) {
        register_t target{address, 0, true};
        auto it = std::lower_bound(m_registers.begin(), m_registers.end(), target);

        if (it == m_registers.end() || it->address != address) {
            it = m_registers.insert(it, register_t{address, 0, true});
        }
        return it->value;
    }

    uint16_t operator[](uint16_t address) const {
        register_t target{address, 0, true};
        auto it = std::lower_bound(m_registers.begin(), m_registers.end(), target);
        
        if (it != m_registers.end() && it->address == address) {
            return it->value;
        }
        return 0; 
    }

    // Handles both inserting new registers and updating existing ones
    void upsert(uint16_t address, uint16_t value) {
        register_t target{address, 0, true};
        
        // O(log N) binary search to find the correct position
        auto it = std::lower_bound(m_registers.begin(), m_registers.end(), target);

        if (it != m_registers.end() && it->address == address) {
            // Case 1: Register already exists, update it in place
            if (it->value == value)
                it->updated = false;
            else
                it->updated = true;
            it->value = value;
        } else {
            // Case 2: Register does not exist, insert it while keeping the vector sorted
            m_registers.insert(it, register_t{address, value, true});
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
        if (m_registers.size() + count > m_registers.capacity()) {
            m_registers.reserve(m_registers.size() + count + 10);
        }

        // 1. Find where the block should start in our sorted vector
        register_t target{start_address, 0, true};
        auto it = std::lower_bound(m_registers.begin(), m_registers.end(), target);

        size_t value_idx = 0;

        // 2. Overwrite existing registers that match the incoming block
        while (it != m_registers.end() && value_idx < count && it->address == (start_address + value_idx)) {
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
        m_registers.insert(it, new_entries.begin(), new_entries.end());
    }

    // O(log N) lookup. Returns true if found, sets the value reference
    register_t* get(uint16_t address) {
        register_t target{address, 0, true};
        auto it = std::lower_bound(m_registers.begin(), m_registers.end(), target);
        
        if (it != m_registers.end() && it->address == address) {
            return &(*it);
        }
        return nullptr;
    }

    const register_t& at(uint16_t n) const {
        return m_registers.at(n);
    }

    void setUpdated(uint16_t n, bool updated) {
        m_registers.at(n).updated = updated;
    }

    size_t size() const {
        return m_registers.size();
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

public:
    DriverAhd();
    ~DriverAhd();

    static void uartRxTask(void *pvParameters);

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