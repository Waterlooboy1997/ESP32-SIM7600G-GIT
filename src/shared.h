#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>
#include <ESP32Time.h>
#include <MCP23018.h>
#include "Sensor.h"
#include "FileWriter.h"
#include "utilities.h"
#include "measurements.h"
#include <Ticker.h>
#include "RTClib.h"

// Enums for modem and client states
enum ModemState {
    POWER_MODEM_ON,
    POWER_MODEM_OFF,
    WAIT_MODEM_ON,
    MODEM_IDLE,
    MODEM_OFF,
    MODEM_HANG
};

enum NetworkState {
    NETWORK_CONNECT,
    NETWORK_CHECK,
    GPRS_CONNECT,
    GPRS_CHECK,
    NETWORK_CONNECTED,
    NETWORK_FAILED,
    NETWORK_HANG
};

enum ClientState {
    CLIENT_IDLE,
    CLIENT_CONNECT,
    CLIENT_CONNECTED,
};
extern NetworkState networkState;

enum class MeasureState {
  IDLE,
  INIT_SENSOR,
  TRIGGER_MEASUREMENT,
  WAIT_FOR_MEASUREMENT,
  GET_MEASUREMENT,
  SAVE_MEASUREMENT,
  CLEANUP
};

// Extern declarations
extern bool manuallyUpdateTimestamp;
extern bool firstVictronMeasurement;
extern int32_t startVoltageUnix;
extern int32_t currentVoltageUnix;
extern float startVoltage;
extern float nowVoltage;
extern int32_t averageCurrent;
extern int32_t averageCurrentCounter;
extern String localIP;
extern bool sleepEnabled;
extern HardwareSerial SerialAT;
extern bool simcomModemPowered;
extern int networkRetries;
extern String apn;
extern String gprsUser;
extern String gprsPass;
extern String server;
extern int port;
extern RTC_DS3231 rtc;
extern ModemState modemState;
extern ClientState clientState;
extern int32_t rowsToUpload;
extern int8_t sampleInterval;
extern int32_t lastRecord;
extern MCP23018 mcp;
extern int SENSOR_TYPE;
extern String sensorType;
extern Sensor* sensor;
extern FileWriter fileWriter;
extern bool modemNetworkEnabled;
extern bool startMeasurement;
extern bool measurementToSave;
extern bool measurementThisScan;
extern SemaphoreHandle_t xMutex;
extern bool otaMode;
extern bool otaInitialized;
extern bool atMode;
extern float currentDraw;
extern float battVolt;
extern float lipoBattVolt;
extern bool rtcInitialised;
extern bool espRtcInitialised;
extern bool mcpInitialised;
extern ESP32Time espRtc;
extern String simCCID;
extern int16_t signalQuality;
extern int8_t wiperRelay;
extern int8_t modbusRelay;
extern uint16_t modbusBaud;
extern uint8_t modbusRx;
extern uint8_t modbusTx;
extern int8_t sdi12Relay;
extern uint8_t sdi12Addr;
extern uint8_t sdi12Pin;
extern uint8_t modemTurnsOffAtMinutes;
extern int32_t LittleFSystemSize;
void modifyConfig(const char* keyPath, JsonVariant value, String reset = "false");
JsonDocument readConfig();
extern float waterLevel;
extern float waterTemp;
extern bool wifiEnabled;
extern String wifiSSID;
extern String wifiPassword;
extern uint16_t wifiPort;
extern uint16_t wifiDurationOn;
extern bool wifiOn;
extern Ticker wifiTimer;
extern bool keepWifiPoweredOn;
extern unsigned long wifiDuration; // Duration in milliseconds
extern bool cameraEnabled;
extern int8_t cameraRelay;
extern int32_t cameraOnTime;
extern int32_t cameraInterval;
extern String batteryMethod;
extern int8_t batteryPin;
extern String loggerId;
extern bool manuallyCheckForServerCommands;
extern bool manuallyCheckTimestampForDataToUpload;
extern bool forceModemOff;
extern float lowScanRate;
extern float highScanRate;
extern float nitrateThreshold;
extern float waterLevelChangeThreshold;
extern float waterLevelThreshold;
extern float lastWaterLevel;
extern float lastNitrateReading;
extern bool otaInProgress;
extern TaskHandle_t measureTriosTask;
extern TaskHandle_t simcomControllerTask;
extern TaskHandle_t lightTimerTask;
extern TaskHandle_t mainTimingControlTask;
extern TaskHandle_t buttonTask;
extern TaskHandle_t serialReadTask;
extern TaskHandle_t batteryMeasureTask;
extern TaskHandle_t cameraTask;
extern TaskHandle_t WiFiTask;
extern TaskHandle_t PPPTask;
#endif // SHARED_H