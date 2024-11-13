See:
https://github.com/Waterlooboy1997/ESP32-SIM7600G-GIT-master


PlatformIO Visual Studio Code Build

Arduino version 3.0

Platformio does not support Arduino version 3.0. In order to utilise the esp-arduino toolkit which includes access to netif and lwip layers for PPP connections, a fork must be included in the platformio.ini file.
platform = https://github.com/tasmota/platform-espressif32/releases/download/2024.09.10/platform-espressif32.zip
'
Notable files:

main.cpp - Initialises the extern variables from shared.h for global use across all .cpp files and is home of setup()

config.cpp - Initialises MCP23018, modbus, real-time clock and reads the configuration file from SPIFFS.

measurements.cpp - Contains simple measurement functions such as reading data from the victron MPPT solar charger, measuring SDI12 sensors, reading 420ma sensors and reading 18650 LIPO and external battery if using a voltage divider.

NicoSensor.cpp - Specific functions for Trios NICO modbus sensor.

OpusSensor.cpp - Specific functions for Trios OPUS modbus sensor.

ppp_client.cpp - Contains all functions relating to the SIM7600G including PPP functionality. (Note: these functions are called from tasks.cpp, see below).

shared.h - Contains all global variables to use across .cpp files.

tasks.cpp - Contains all tasks.

utilities.cpp - Contains tonne of major functions like reading JSON, returning parameters, processing commands etc.

utilities.h - This is where a majority of pin declarations are made. 
