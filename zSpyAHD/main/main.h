#pragma once

#include "driverAhd.h"
#include "driver/uart.h"
#include "driver/gpio.h"

//#include "aldesDriver.h"


#define UART0_NUM            UART_NUM_0

#define UART0_TX_PIN         GPIO_NUM_16   // Change as needed
#define UART0_RX_PIN         GPIO_NUM_17   // Change as needed
#define UART0_RTS_PIN        UART_PIN_NO_CHANGE  // Use a valid GPIO pin for RTS
#define UART0_CTS_PIN        UART_PIN_NO_CHANGE
#define UART0_BAUD_RATE      9600
#define UART0_BUFFER_SIZE    1024

// Main class used for testing only
class Main final
{


public:
    Main();
    void run(void);
    void setup(void);

private:
    DriverAhd* _ahd = nullptr;

    uint8_t buffer[UART0_BUFFER_SIZE];

}; // Main Class