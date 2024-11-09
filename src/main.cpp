/* -------------------------------------------------------------------------- */
/*                            ESP32 Sensor Platform                           */
/*                                                                            */
/* This code implements a multi-sensor platform using an ESP32 microcontroller*/
/* It includes functionality for various sensors, GSM communication, file     */
/* management, and power management.                                          */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/*                                  BackTrace                                 */
/*C:\Users\jc321293\.espressif\tools\xtensa-esp-elf\esp-13.2.0_20240305\xtensa-esp-elf\bin\xtensa-esp32-elf-addr2line -pfiaC -e .pio\build\esp-wrover-kit\firmware.elf 0x400d7304:0x3ffcb0e0 */
/* -------------------------------------------------------------------------- */
//0x4008d357:0x3ffd3550 0x4008d46f:0x3ffd3580 0x4008d135:0x3ffd35a0 0x4008d292:0x3ffd35e0 0x400dbfbd:0x3ffd3600 0x400dd337:0x3ffd3650 0x400db071:0x3ffd37a0
//0x400d67f8:0x3ffdddc0
//0x4008d357:0x3ffb5210 0x4008d46f:0x3ffb5240 0x4008d135:0x3ffb5260 0x4008d292:0x3ffb52a0 0x400dbfcd:0x3ffb52c0 0x400dd3a7:0x3ffb5310 0x400de4ad:0x3ffb5460 0x400dbb17:0x3ffb5600
//ESPTOOL
// 0x400832d9:0x3ffd2e70 0x4008ebdd:0x3ffd2e90 0x40091bf5:0x3ffd2eb0 0x401106d2:0x3ffd2f20 0x40110805:0x3ffd2fc0 0x40110442:0x3ffd3030 0x40110183:0x3ffd3110 0x4010f6e6:0x3ffd3140 0x400deb40:0x3ffd3180 0x400d6328:0x3ffd32b0
//C:\Users\jc321293\.espressif\python_env\idf5.3_py3.11_env\Scripts\esptool.py
/* -------------------------------------------------------------------------- */
/*                                  Libraries                                 */
/* -------------------------------------------------------------------------- */
#include "config.h"
#include "measurements.h"
#include "tasks.h"
#include "shared.h"

// Core functionality
#include <Arduino.h>
#include "LittleFS.h"
#include "utilities.h"
#include <Update.h>
#include <esp_sleep.h>
#include "esp_system.h"
#include "esp_pm.h"

// Communication protocols
#include <SPI.h>
#include <Wire.h>
#ifndef PPP_mode
#include <TinyGsmClient.h>  // Ensure this path is correct
#endif
#include "time.h"

// Sensor and peripheral libraries
#include <MCP23018.h>
#include <RTClib.h>

// Utility libraries
#include <Ticker.h>
#include "FileWriter.h"
#include <ArduinoJson.h>
#include <vector>
#include "Sensor.h"
#include "ppp_client.h"
/* -------------------------------------------------------------------------- */
/*                            Configurable Settings                           */
/* -------------------------------------------------------------------------- */

// These variables will be updated by reading the /config files
int8_t sampleInterval = 60;  // Sampling interval in minutes
int32_t lastRecord = 1;      // Record keeping counter

/* -------------------------------------------------------------------------- */
/*                         Task Handles and Priorities                        */
/* -------------------------------------------------------------------------- */

/*
Task priorities:
 handleServerTask - core 1, priority 10
 mainTimingControlTask - core 1, priority 5
 measureTriosTask - core 1, priority 4
 serialReadTask - core 1, priority 3
 buttonTask - core 1, priority 2
 backgroundHealthCheckTask - core 1, priority 1
 lightTimerTask - core 0, priority 0
*/

TaskHandle_t measureTriosTask;
TaskHandle_t simcomControllerTask;
TaskHandle_t lightTimerTask;
TaskHandle_t mainTimingControlTask;
TaskHandle_t buttonTask;
TaskHandle_t serialReadTask;
TaskHandle_t batteryMeasureTask;
TaskHandle_t cameraTask;
TaskHandle_t WiFiTask;
TaskHandle_t PPPTask;


/* -------------------------------------------------------------------------- */
/*                           Class Initializations                            */
/*                           All these variables below 
                      are setup with dummy default values                     */
/* -------------------------------------------------------------------------- */
/**/
int32_t LittleFSystemSize = 0;
// Define instances
bool manuallyUpdateTimestamp = false;
int32_t startVoltageUnix = 0;
int32_t currentVoltageUnix = 0;
int32_t averageCurrent = 0;
int32_t averageCurrentCounter = 0;
float startVoltage = 0;
float nowVoltage = 0;
bool firstVictronMeasurement = true;
bool forceModemOff = false;
bool manuallyCheckForServerCommands = false;
bool manuallyCheckTimestampForDataToUpload = false;
String loggerId = "300";
String localIP = "0.0.0.0";
#ifndef PPP_mode
HardwareSerial SerialAT(2);
TinyGsm modem(SerialAT);
TinyGsmClient client(modem, 0);
#endif
uint8_t sdi12Pin = 18;
ESP32_SDI12 sdi12(sdi12Pin);
//Wifi Declarations
bool wifiEnabled = true;
String wifiSSID = "ESP32_308";
String wifiPassword = "ofbts84710";
uint16_t wifiPort = 80;
uint16_t wifiDurationOn = 0;
bool wifiOn = false;
Ticker wifiTimer;
unsigned long wifiDuration = 0;
bool keepWifiPoweredOn = true;
bool cameraEnabled = false;
bool sleepEnabled = false;
int8_t cameraRelay = 2;
int32_t cameraOnTime = 0;
int32_t cameraInterval = 0;
// Define the extern variables
bool simcomModemPowered = false;
int networkRetries = 0;
String sensorType = "OPUS";
String apn = "telstra.extranet";
String gprsUser = "";
String gprsPass = "";
String server = "aws-loggernet-server.ddns.net";
int port = 305;
float waterLevel = 0.00;
float waterTemp = 0.00;
float lipoBattVolt = 0.00;
float battVolt = 0.00;
float currentDraw = 0.00;
String simCCID = "No sim installed";
int16_t signalQuality = 0;
int8_t wiperRelay = 1;
int8_t modbusRelay = 1;
uint16_t modbusBaud = 9600;
uint8_t modbusRx = 19;
uint8_t modbusTx = 23;
int8_t sdi12Relay = 1;
uint8_t sdi12Addr = 0;
RTC_DS3231 rtc;
ESP32Time espRtc(0);  // offset in seconds GMT+10
ModemState modemState = POWER_MODEM_ON;
NetworkState networkState = NETWORK_HANG;
ClientState clientState = CLIENT_IDLE;
int32_t rowsToUpload = 0;
MCP23018 mcp(MCP_ADDRESS);
String batteryMethod = "adc";
int8_t batteryPin = 39;
bool mcpInitialised = false;
int SENSOR_TYPE = SENSOR_TYPE_OPUS;
Sensor* sensor = nullptr;
FileWriter fileWriter;
bool modemNetworkEnabled = false;
bool startMeasurement = false;
bool measurementToSave = false;
bool measurementThisScan = false;
SemaphoreHandle_t xMutex;
bool otaMode = false;
bool otaInitialized = false;
bool atMode = false;
bool rtcInitialised = false;
bool espRtcInitialised = false;
uint8_t modemTurnsOffAtMinutes = 15;
float lowScanRate = 60;
float highScanRate = 15;
float nitrateThreshold = 1.0;
float waterLevelChangeThreshold = 0.03;
float waterLevelThreshold = 1.0;
float lastWaterLevel = 0.1;
float lastNitrateReading = 0.00;
bool otaInProgress = false;

/* -------------------------------------------------------------------------- */
/*                                    Setup                                   */
/* -------------------------------------------------------------------------- */
/* Main program */
void setup() {
  // Initialize mutex
  xMutex = xSemaphoreCreateMutex();

  // Pinmode LED pin & relay
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Setting up serial...
  Serial.begin(115200);
  Serial.println("SETUP: Serial Port 115200 baud...V2");

  // Seeting up SDI12...
  sdi12.begin();
  Serial.println("SETUP: SDI12 init...");

  int freq = getCpuFrequencyMhz(); // Get the CPU frequency in MHz
  Serial.print("Get CPU Frequency: ");
  Serial.print(freq);
  Serial.println(" MHz");
  setCpuFrequencyMhz(240); // Set the CPU frequency to 240 MHz
  Serial.println("Set CPU Frequency set to 240 MHz");
  freq = getCpuFrequencyMhz(); // Get the CPU frequency in MHz
  Serial.print("Get CPU Frequency: ");
  Serial.print(freq);
  Serial.println(" MHz");

  // Setting up modem
  #ifndef PPP_mode
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("SETUP: Modem port 115200 baud...");
  #endif

  Serial.println("Setting compile time...");
  setTimeFromCompile();

  //Turn wifi on at setup for 5 minutes
  turnOnWiFiForDuration(300000);

  Serial.println("SETUP: Initialising LittleFS...");
  loadConfig();

  Serial.println("SETUP: Initialising RTC...");
  initRTC();
  espRtcInitialised = true;
  Serial.print("SETUP: ESP32 Timestamp: ");
  Serial.println(espRtc.getTime("%F %T"));

  //Read variables from LittleFS storage
  initMCP();

  //40k total stack size in words for 160kb     6,1,0,3,2
  xTaskCreatePinnedToCore(WiFiTaskCode, "WiFiTask", 10000, NULL, 8, &WiFiTask, 1);
  xTaskCreatePinnedToCore(mainTimingControlTaskCode, "mainTimingControlTask", 10000, NULL, 9, &mainTimingControlTask, 1);
  xTaskCreatePinnedToCore(measureTriosTaskCode, "measureTriosTask", 10000, NULL, 10, &measureTriosTask, 1);
  xTaskCreatePinnedToCore(simcomControllerTaskCode, "simcomControllerTask", 25000, NULL, 6, &simcomControllerTask, 1);
  xTaskCreatePinnedToCore(PPPTaskCode, "PPPTask", 25000, NULL, 7, &PPPTask, 1);

  xTaskCreatePinnedToCore(serialReadTaskCode, "serialReadTask", 5000, NULL, 2, &serialReadTask, 0);
  xTaskCreatePinnedToCore(buttonTaskCode, "buttonTaskCode", 6000, NULL, 1, &buttonTask, 0);
  xTaskCreatePinnedToCore(lightTimerTaskCode, "lightTimerTask", 4000, NULL, 0, &lightTimerTask, 0);
  xTaskCreatePinnedToCore(batteryMeasureTaskCode, "victronMeasureTask", 3000, NULL, 3, &batteryMeasureTask, 0);
  xTaskCreatePinnedToCore(cameraTaskCode, "cameraTask", 3000, NULL, 4, &cameraTask, 0);

  digitalWrite(LED_PIN, LOW);
}

void loop() {
  vTaskDelete(NULL);
}
