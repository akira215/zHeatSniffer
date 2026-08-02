#pragma once


#include <vector>

// Driver class used for testing only
class DriverDaikin final
{
    static const uint8_t CRC_TABLE[];
public:
    DriverDaikin();
    ~DriverDaikin();

    void callback(const std::vector<uint8_t>& buffer);
};
