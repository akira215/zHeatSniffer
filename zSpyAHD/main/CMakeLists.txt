set(SOURCES main.cpp
            driverAhd.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . 
                    REQUIRES periodicTask cppgpio)
                    