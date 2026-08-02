#pragma once

#include "driverHomebus.h"
#include "driverDaikin.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// Main class used for testing only
class Main final
{

public:
    Main();
    void run(void);
    void setup(void);

private:
    DriverHomebus* _hbs     = nullptr;
    DriverDaikin* _daikin   = nullptr;

    uint8_t buffer[100];

}; // Main Class