#pragma once

#include "cppgpio.h" // to del
#include "buttonTask.h" // to del
#include "blinkTask.h" // to del
#include "ws2812.h" // to del


#define LINE_BUF_SIZE     256

#define MIN_WAIT_MS       50
#define MAX_WAIT_MS       1500

#define UART0_NUM            UART_NUM_0
#define UART1_NUM            UART_NUM_1
/*
#define UART1_TX_PIN         GPIO_NUM_16   // Change as needed
#define UART1_RX_PIN         GPIO_NUM_17   // Change as needed
#define UART1_RTS_PIN        GPIO_NUM_19
*/

#define UART0_TX_PIN         GPIO_NUM_16   // Change as needed
#define UART0_RX_PIN         GPIO_NUM_17   // Change as needed
#define UART0_RTS_PIN        UART_PIN_NO_CHANGE  // Use a valid GPIO pin for RTS
#define UART0_CTS_PIN        UART_PIN_NO_CHANGE
#define UART0_BAUD_RATE      9600
#define UART0_BUFFER_SIZE    1024

#define UART1_TX_PIN        GPIO_NUM_18 
#define UART1_RX_PIN        GPIO_NUM_19

// Main class used for testing only
class Main final
{
    GpioInput               _button {static_cast<gpio_num_t>(CONFIG_PIN_BUTTON), true};
    ButtonTask*             _buttonTask = nullptr;
    Ws2812                  _neoLed{static_cast<gpio_num_t>(8)};

    static BlinkTask*       _ledBlinking;
    static GpioOutput       _led;
    
    static GpioOutput       _rts;

    // As hardware is planned for 2 uart, configure 2nd uart pin as input and output
    //GpioOutput              _tx1 {(gpio_num_t)10};
    //GpioInput               _rx1 {(gpio_num_t)11};
    //GpioOutput              _rts1 {(gpio_num_t)1};

public:
    Main();
    void run(void);
    void setup(void);

    static void shortPressHandler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    static void longPressHandler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);


private:
    /// @brief Helper to flash led
    /// @param speed flash cycle in ms. if 0, led will be set to off, 
    /// if -1 led will be switch on
    static void ledFlash(uint64_t speed);

    int uart1_read(uint8_t *data, int max_len, int timeout_ms);
    int uart1_write(const uint8_t *data, int len);
    int uart1_read_available(uint8_t *data, int max_len);

    void init_spiffs(void);
    void uart_file_sender_task(const char *pvParameters);

    uint8_t buffer[UART0_BUFFER_SIZE];

    TaskHandle_t                _xButtonHandle      = nullptr;
    TaskHandle_t                _eventLoopHandle    = nullptr;

}; // Main Class