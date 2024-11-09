#ifndef UTILITIES_H
#define UTILITIES_H

#define PPP_mode 1

#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <WString.h>
#include <Update.h>

// Define SDI12 data pin and other necessary constants
#define SDI12_DATA_PIN 18
#define LED_PIN 12
#define MODBUS_RX 19
#define MODBUS_TX 23
#define SBLT_PIN 39
#define VICTRON_RX 5

// Fixed Pins for LilyGo SIM7600G
#define UART_BAUD 115200
#define MODEM_TX 27
#define MODEM_RX 26
#define MODEM_PWRKEY 4
#define MODEM_DTR 32
#define MODEM_RI 33
#define MODEM_FLIGHT 25
#define MODEM_STATUS 34
#define BAT_ADC 35
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

// Trios MODBUS address
#define MODBUS_ADDRESS 1

//Modes
#define ENABLE_SLEEP_MODE
#define RELEASE_VERSION
//#define TEST_VERSION

// Simcom Buffers
#define TINY_GSM_RX_BUFFER 1024
#define TINY_GSM_MODEM_SIM7600

// Program ENUMS
#define SENSOR_TYPE_OPUS 1
#define SENSOR_TYPE_NICO 2

// SIM7600 Modem Definitions
#define TINY_GSM_MODEM_SIM7600

// MCP Address
#define MCP_ADDRESS 0x20

// Function Declarations
ArduinoJson::V710PB22::JsonDocument returnJsonParameters();
ArduinoJson::V710PB22::JsonDocument processCommand(String command);
ArduinoJson::V710PB22::JsonDocument readJsonData(const String& filename, const String& startTime, int lastNRows, int startRow = 0, int endRow = -1);
time_t parseTimestamp(const char* timestamp);
int8_t getHour();
int8_t getMinute();
String getFormattedTimestamp();
float getLittleFsFillPercent();
bool isClientConnected();
void checkMemory();
void setTimeFromCompile();
void appendToJson(const String& filename, const JsonDocument& entry); 
void turnOffWiFi();
void turnOnWiFiForDuration(unsigned long duration);
void toggleRelay(int8_t relayNumber, uint8_t state);
void resetJsonFile(const String& filename, const String& jsonString);
void printHeapInfo(const char* message);
void adjustTimestamp(uint32_t timestamp);
void printPartitions();
#endif // UTILITIES_H