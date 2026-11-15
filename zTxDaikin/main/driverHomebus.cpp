
#include <esp_log.h>

#include "driver/rmt_encoder.h"
#include "DriverHomebus.h"

static const char *TAG = "DriverHomebus";

// Static ISR handler
bool IRAM_ATTR DriverHomebus::rxRmtIsr(rmt_channel_handle_t channel,
                                                const rmt_rx_done_event_data_t *edata,
                                                void *user_data)
{
    DriverHomebus* instance = (DriverHomebus*)user_data;

    if (edata->num_symbols > 0) {
        if (edata->num_symbols <= instance->_rxBufferSize) {
            // Mark current write buffer as ready for reading
            instance->_readIdx = instance->_writeIdx;
            instance->_nSymbols = edata->num_symbols;
            // Swap to other buffer for next receive
            instance->_writeIdx ^= 1;
        } else {
            instance->_overflow = true;
        }
    }

    // Immediately restart reception on the next buffer (ISR-safe in ESP-IDF 5.x)
    rmt_receive(instance->_rx, instance->_rxBuffers[ instance->_writeIdx],
                  instance->_rxBufferSize * sizeof(rmt_symbol_word_t), &instance->_rxCfg);

    return false;  // No high priority task woken
}

DriverHomebus::DriverHomebus()
{
    
    // Allocate RX buffers
    for (int i = 0; i < 2; i++) {
        _rxBuffers[i] = (rmt_symbol_word_t *)heap_caps_calloc(
                            RX_BUFFER_SYMBOLS, sizeof(rmt_symbol_word_t), MALLOC_CAP_8BIT);
        if (_rxBuffers[i] == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate RX buffer %d", i);
            return;
        }
    }
    _rxBufferSize = RX_BUFFER_SYMBOLS;
    _writeIdx = 0;
    _readIdx = 0xFF;  // No data ready
    _overflow = false;
    _nSymbols = 0;
    
    
    // Initialize RMT TX Channel for sending P1/P2 frames
    rmt_tx_channel_config_t tx_config{};
    tx_config.gpio_num =  static_cast<gpio_num_t>(HBS_TX_GPIO);
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = HOMEBUS_RMT_RESOLUTION_HZ;
    tx_config.mem_block_symbols = 64;
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = 0;

    esp_err_t err = rmt_new_tx_channel(&tx_config, &_tx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create TX channel: %s", esp_err_to_name(err));
        return;
    }

    // Create copy encoder for raw symbol transmission
    rmt_copy_encoder_config_t encoder_config = {};
    err = rmt_new_copy_encoder(&encoder_config, &_txEncoder);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create TX encoder: %s", esp_err_to_name(err));
        return;
    }


    // Initialize RMT RX Channel for decoding incoming P1/P2 waveforms
    rmt_rx_channel_config_t rx_config{};
    rx_config.gpio_num = static_cast<gpio_num_t>(HBS_RX_GPIO);
    rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_config.resolution_hz = HOMEBUS_RMT_RESOLUTION_HZ;  
    rx_config.mem_block_symbols = 64;
    rx_config.flags.invert_in = 0;
    rx_config.flags.with_dma = false;
    

    err = rmt_new_rx_channel(&rx_config, &_rx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create RX channel: %s", esp_err_to_name(err));
        return;
    }

    // Register RX done callback
    rmt_rx_event_callbacks_t rx_cbs = {};
    rx_cbs.on_recv_done = rxRmtIsr;
    err = rmt_rx_register_event_callbacks(_rx, &rx_cbs, this);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register RX callbacks: %s", esp_err_to_name(err));
        return;
    }

    // Enable Tx and Rx
    err = rmt_enable(_rx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable RX channel: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_enable(_tx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(err));
        return;
    }

    // Transmit dummy byte to initialize TX line high
    rmt_symbol_word_t tx_items[1] = {};
    tx_items[0].duration0 = HOMEBUS_BIT_DURATION * 12;
    tx_items[0].level0 = 1;
    tx_items[0].duration1 = 1;
    tx_items[0].level1 = 1;

    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;
    tx_cfg.flags.eot_level = 1;  // idle high
    rmt_transmit(_tx, _txEncoder, tx_items, sizeof(tx_items), &tx_cfg);
    rmt_tx_wait_all_done(_tx, portMAX_DELAY);


    ESP_LOGI(TAG, "Tx and Rx RMT initialized");

    //Reserve to avoid heap allocation
    _registerMap.reserve(REGISTER_MAP_SIZE);

}

DriverHomebus::~DriverHomebus()
{
     if (_txEncoder) {
        rmt_del_encoder(_txEncoder);
        _txEncoder = nullptr;
    }
    if (_tx) {
        rmt_disable(_tx);
        rmt_del_channel(_tx);
        _tx = nullptr;
    }
    if (_rx) {
        rmt_disable(_rx);
        rmt_del_channel(_rx);
        _rx = nullptr;
    }
   

    for (int i = 0; i < 2; i++) {
        if (_rxBuffers[i]) {
            heap_caps_free(_rxBuffers[i]);
            _rxBuffers[i] = nullptr;
        }
    }

}

void DriverHomebus::sendBytes(const std::vector<uint8_t>& datas)
{
    if((!_tx)||(!_txEncoder)) {
        ESP_LOGE(TAG, "Unable to sendBytes - TX is not well initialized");
        return;
    }
    static std::vector<rmt_symbol_word_t> symbols;
    symbols.clear();
    symbols.reserve(datas.size() * 11 + 11); // stop + 8 bits + parity +  stop bit

    for (uint8_t byte : datas) {

        symbols.push_back(hbs_bit0); // Start bit
        bool parity = 0;
        for (int bit = 0; bit < 8; bit++)
        {
            // LSB for Homebus
            bool tx_bit = (byte >> bit) & 0x01;
            parity ^= tx_bit;
            symbols.push_back(tx_bit ? hbs_bit1 : hbs_bit0);
        }
        
        // Even Parity
        symbols.push_back(parity ? hbs_bit1 : hbs_bit0);
        // Stop
        symbols.push_back(hbs_bit1);
    }
    

    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;
    tx_cfg.flags.eot_level = 1;  // idle high

     // Check if TX is ongoing
    esp_err_t err = rmt_tx_wait_all_done(_tx, 0);

    if (err == ESP_ERR_TIMEOUT) {
        // Transmission is ONGOING

        // Wait if previous transmission is still ongoing
        esp_err_t err = rmt_tx_wait_all_done(_tx, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to wait for previous transmission to complete: %s", esp_err_to_name(err));
            return;
        }
        // RESET > 50 µs
        esp_rom_delay_us(60);
    }
    

    err = rmt_transmit(_tx, _txEncoder,
                                    symbols.data(),
                                    symbols.size() * sizeof(rmt_symbol_word_t),
                                    &tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX transmit failed: %s", esp_err_to_name(err));
        return;
    }


}

void DriverHomebus::start()
{
    // Set RST pin to low
    _enable.on();

    _rxCfg.signal_range_min_ns = 1000;  // filter noise < 1us
    _rxCfg.signal_range_max_ns = (HOMEBUS_BIT_DURATION * 12) * 1000000000ULL / HOMEBUS_RMT_RESOLUTION_HZ;

    esp_err_t err = rmt_receive(_rx,
                                _rxBuffers[_writeIdx],
                                _rxBufferSize * sizeof(rmt_symbol_word_t),
                                &_rxCfg);
                               
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RMT receive: %s", esp_err_to_name(err));
    }
   
    // Clear screen and home cursor once at startup
    printf("\e[2J\e[H");
}


void DriverHomebus::run()
{
    if (_overflow){
        ESP_LOGW(TAG, "RX buffer overflow");
        _overflow = false;
        return;
    }

    // Check if a buffer is ready for processing
    uint8_t currentIdx = _readIdx;
    if (currentIdx == 0xFF) {
        return;
    }

    // Capture state and mark as processed
    // Note: Reception already restarted in ISR callback on the other buffer
    size_t size = _nSymbols;
     _readIdx = 0xFF;
    _nSymbols = 0;

    // Process received data from the completed buffer
    std::vector<uint8_t> data = decodeRmtData(_rxBuffers[currentIdx], size);

    // Filter for potential noise/invalid packets. We need at least the header present
    if (data.size() < 3)
        return;

    printPacket(data);

    // Pass buffer up stack
    /*
    if (this->callback_)
    {
        this->callback_(this->callback_arg_, buffer, decoded_size);
    }
        */
}

//static
std::vector<uint8_t> DriverHomebus::decodeRmtData(rmt_symbol_word_t *symbols, size_t num_symbols)
{
    std::vector<uint8_t> result;
    result.reserve(DECODE_BUFFER_SIZE);

    unsigned int symbol_index = 0;

    while (symbol_index < num_symbols) {
        // Sample towards the start of a half bit
        unsigned int accumulator = 0;
        unsigned int sampling_time = HOMEBUS_HALF_BIT_DURATION / 3;
        uint16_t decoded_byte = 0;
        size_t bit_pos = 0;
        while (sampling_time < (HOMEBUS_BIT_DURATION * (11))) {
            if (symbol_index > num_symbols) {
                ESP_LOGE(TAG, "Invalid symbol index %lu > %lu", symbol_index, num_symbols);
                ESP_LOGE(TAG, "byte_pos = %lu, bit_pos = %lu, accumulator = %lu", result.size(), bit_pos, accumulator);

                return  result;
            }

            rmt_symbol_word_t *symbol = &symbols[symbol_index];

            if ((symbol_index <= num_symbols)) {

                if (symbol->duration0 > (HOMEBUS_BIT_DURATION * (11))) {
                    ESP_LOGE(TAG, "Invalid symbol duration0: %lu > %lu", symbol->duration0, (HOMEBUS_BIT_DURATION * (11)));
                    return result;
                }
                if (symbol->duration1 > (HOMEBUS_BIT_DURATION * (11))) {
                    ESP_LOGE(TAG, "Invalid symbol duration1: %lu > %lu", symbol->duration1, (HOMEBUS_BIT_DURATION * (11)));
                    return result;
                }

                accumulator += symbol->duration0;
                while ((sampling_time < accumulator) && (bit_pos < 11)) {
                    sampling_time += HOMEBUS_BIT_DURATION;
                    decoded_byte >>= 1;
                    bit_pos++;
                }

                accumulator += symbol->duration1;
            }
            // Pad out accumulator if we've timed out
            if ((symbol_index == num_symbols) || (symbol->duration1 == 0)) {
                    accumulator += HOMEBUS_BIT_DURATION * 11;
            }

            while ((sampling_time < accumulator) && (bit_pos < 11)) {
                    sampling_time += HOMEBUS_BIT_DURATION;
                    decoded_byte = (decoded_byte >> 1) | (1 << 10);
                    bit_pos++;
            }

            symbol_index++;
        } //while (sampling_time < (HOMEBUS_BIT_DURATION * (11)))

        /* Check framing and parity */
        if ((decoded_byte & 0x001) == 0x001) {
            ESP_LOGE(TAG, "Bad Start framing, %lu (byte:0x%03x, idx=%u, len=%u)", accumulator, decoded_byte, symbol_index, result.size());
            printPacket(result);
            result.clear();
            return result;
        }
        if ((decoded_byte & 0x400) != 0x400) {
            ESP_LOGE(TAG, "Bad Stop framing, %lu (byte:0x%03x, idx=%u, len=%u)", accumulator, decoded_byte, symbol_index, result.size());
            printPacket(result);

            for(int i = 0; i < 18; i++)
                ESP_LOGE(TAG, "  symbol[%u](dur0=%u, dur1=%u)", i, symbols[i].duration0, symbols[i].duration1);
    
            result.clear();
            return result;
        }
        if ((__builtin_popcount(decoded_byte) & 1) == 0) {
            ESP_LOGE(TAG, "Bad parity");
            result.clear();
            return result;
        }

        decoded_byte >>= 1;
        //result.push_back(static_cast<uint8_t>(decoded_byte & 0xFF));
        result.push_back(static_cast<uint8_t>(decoded_byte));

        if (result.size()+1 >= DECODE_BUFFER_SIZE) {
            ESP_LOGE(TAG, "Over max length: %lu > %lu", result.size()+1, DECODE_BUFFER_SIZE);
            result.clear();
            return result;
        }
    } // while (symbol_index < symbol_num)



    return result;
}

//static 
void DriverHomebus::printPacket(const std::vector<uint8_t>& data)
{
    // Each byte needs 3 chars ("xx "), plus prefix (~15) and suffix
    static constexpr size_t MAX_PRINT_BYTES = 48;
    char str[16 + MAX_PRINT_BYTES * 3 + 2];
    char *p = str;

    size_t print_len = (data.size() > MAX_PRINT_BYTES) ? MAX_PRINT_BYTES : data.size();
    p += sprintf(str, "len=%02d pkt=[", data.size());
    for (size_t i = 0; i < print_len; i++) {
        p += sprintf(p, "%02x ", data[i]);
    }
    if (print_len > 0) {
        *(p - 1) = ']';
    } else {
        *p++ = ']';
    }
    *p = 0;
    ESP_LOGV(TAG, "%s", str);
}

//
void DriverHomebus::printDashboard()
{
    // Move cursor to top-left corner without clearing
    printf("\e[H");
    /*

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
    */

}