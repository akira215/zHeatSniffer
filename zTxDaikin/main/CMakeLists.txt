set(SOURCES main.cpp
            driverHomebus.cpp
            driverDaikin.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . 
                    REQUIRES periodicTask cppgpio esp_driver_rmt spiffs)

# Automatically flashes the 'spiffs_image' directory to the 'spiffs' partition
spiffs_create_partition_image(spiffs ../spiffs_image FLASH_IN_PROJECT)
                    