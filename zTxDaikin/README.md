# zTxDaikin

This repo is the source firmware developped only to send TX simulated Dakini frame frames over RMT

## data file

Data file (to be sent via RMT) shall be located in the folder `spiffs_image` directly in the root folder of the project

To flash it directly to the device:
 
 * Create partition named `spiffs` type `data` subtype `spiffs`

 * In the CMakeLists of the main folder place those line at the end:

```CMakeLists.txt
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . 
                    REQUIRES periodicTask cppgpio esp_driver_rmt spiffs)

# Automatically flashes the 'spiffs_image' directory to the 'spiffs' partition
spiffs_create_partition_image(spiffs ../spiffs_image FLASH_IN_PROJECT)
                    
```

## Usage and Examples





