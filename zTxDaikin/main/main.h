#pragma once

#include <string>

#include "driverHomebus.h"
//#include "driver/uart.h"
#include "driver/gpio.h"


#define LINE_BUF_SIZE     256

#define MIN_WAIT_MS       1
#define MAX_WAIT_MS       1000

// Main class used for testing only
class Main final
{
    DriverHomebus* _hbs = nullptr;
    uint8_t buffer[100];

public:
    Main();
    void run(void);
    void setup(void);

private:

    void SendRmtData(const std::string& file);
    void init_spiffs();

}; // Main Class