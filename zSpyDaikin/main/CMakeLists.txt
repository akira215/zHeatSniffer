set(SOURCES main.cpp
            driverHomebus.cpp
            driverDaikin.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . 
                    REQUIRES periodicTask cppgpio esp_driver_rmt)
                    