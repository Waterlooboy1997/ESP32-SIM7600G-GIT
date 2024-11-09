#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "LittleFS.h"
#include "FileWriter.h"
#include "Sensor.h"
#include "OpusSensor.h"
#include "NicoSensor.h"
#include <ESP32Time.h>
#include <MCP23018.h>
#include "utilities.h"
#include "shared.h"

void initMCP() {
  Wire.begin();

  for (int8_t i = 0; i < 5; i++) {
    if (!mcp.Init()) {
      Serial.println("SETUP: MCP not initialised...");
      mcpInitialised = false;
    } else {
      Serial.println("SETUP: MCP Initialised...");
      mcpInitialised = true;
      // Initialize switch inputs
      mcp.setPinX(7, A, INPUT_PULLUP, LOW);
      mcp.setPinX(7, B, INPUT_PULLUP, LOW);

      // Setup remaining pins
      mcp.setPinX(0, A, INPUT, LOW);
      mcp.setPinX(1, A, INPUT, LOW);
      mcp.setPinX(2, A, INPUT, LOW);
      mcp.setPinX(3, A, INPUT, LOW);
      mcp.setPinX(4, A, INPUT, LOW);
      mcp.setPinX(5, A, INPUT, LOW);
      mcp.setPinX(6, A, INPUT, LOW);
      mcp.setPinX(0, B, INPUT, LOW);
      mcp.setPinX(1, B, INPUT, LOW);
      mcp.setPinX(2, B, INPUT, LOW);
      mcp.setPinX(3, B, INPUT, LOW);
      mcp.setPinX(4, B, INPUT, LOW);
      mcp.setPinX(5, B, INPUT, LOW);
      mcp.setPinX(6, B, INPUT, LOW);
      mcpInitialised = true;
      break;
    }
  }
}

void initModbus(bool testCommunication) {

  if (SENSOR_TYPE == SENSOR_TYPE_OPUS) {
    sensor = new OpusSensor(MODBUS_ADDRESS, Serial1);
  } else if (SENSOR_TYPE == SENSOR_TYPE_NICO) {
    sensor = new NicoSensor(MODBUS_ADDRESS, Serial1);
  } else {
    sensor = new OpusSensor(MODBUS_ADDRESS, Serial1);
  }

  if (sensor)
  {
    sensor->initialize(modbusBaud, modbusRx, modbusTx);
  }
  Serial.println("SETUP: MB port 9600 baud...");

  if(testCommunication){
    char firmwareVersion[11];
    uint32_t systemDateTime;

    if (sensor->readFirmwareVersion(firmwareVersion)) {
      Serial.print("Firmware Version: ");
      Serial.println(firmwareVersion);
    } else {
      Serial.println("Failed to read firmware version");
    }

    if (sensor->readSystemDateTime(systemDateTime)) {
      Serial.print("System DateTime: ");
      Serial.println(systemDateTime);
      if (systemDateTime > 1717920360) { //Unix value is 09-06-2024
        //espRtc.setTime(systemDateTime + 36000); //Add +10:00 hours for GMT
      }
    } else {
      Serial.println("Failed to read system date and time");
    }
    }
}

void initRTC() {
  Serial.println("SETUP: Initialising RTC...");
  
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    rtcInitialised = false;
    return;
  }
  
  if (rtc.lostPower()) {
    Serial.println("WARNING: RTC lost power, let's set the time!");
    rtcInitialised = false;
    // If the RTC lost power, you might want to set the time here. Example:
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    return;
  }

  rtcInitialised = true;  // Update the initialization status
  
  // Optionally, you could synchronize ESP32's internal RTC time with DS3231 time
  DateTime now = rtc.now();
  espRtc.setTime(now.unixtime()); // Assuming 'espRtc' is an instance of a class that handles the ESP32's internal RTC
  
  Serial.print("SETUP: RTC Timestamp: ");
  Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
}

// Example loadConfig function to read config.json
void loadConfig() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println("Failed to parse config file");
    configFile.close();
    return;
  }
  configFile.close();

  // Assign configSettings
  sampleInterval = doc["configSettings"]["sampleInterval"];
  sleepEnabled = doc["configSettings"]["sleepEnabled"];
  String ioExpanderAddress = doc["classInitializations"]["ioExpander"]["address"].as<String>();
  loggerId = doc["configSettings"]["id"].as<String>();

  lowScanRate = doc["dynamicSampleIntervals"]["lowScanRate"];
  highScanRate = doc["dynamicSampleIntervals"]["highScanRate"];
  nitrateThreshold = doc["dynamicSampleIntervals"]["nitrateThreshold"];
  waterLevelChangeThreshold = doc["dynamicSampleIntervals"]["waterLevelChangeThreshold"];
  waterLevelThreshold = doc["dynamicSampleIntervals"]["waterLevelThreshold"];

  // Modbus init
  wiperRelay = doc["classInitializations"]["wiper"]["relay"];
  modbusRelay = doc["classInitializations"]["modbus"]["relay"];
  sensorType = doc["classInitializations"]["modbus"]["sensorType"].as<String>();
  modbusBaud = doc["classInitializations"]["modbus"]["baud"];
  modbusRx = doc["classInitializations"]["modbus"]["rx"];
  modbusTx = doc["classInitializations"]["modbus"]["tx"];
  if (sensorType == "OPUS") {
    SENSOR_TYPE = SENSOR_TYPE_OPUS;
  } else if (sensorType == "NICO") {
    SENSOR_TYPE = SENSOR_TYPE_NICO;
  } else {
    SENSOR_TYPE = SENSOR_TYPE_OPUS; //default to OPUS if error in reading occurs
  }
  Serial.print("Sensor type: ");
  Serial.println(SENSOR_TYPE);
  initModbus(false);

  //SDI12 init
  sdi12Relay = doc["classInitializations"]["sdi12"]["relay"];
  sdi12Pin = doc["classInitializations"]["sdi12"]["pin"];
  sdi12Addr = doc["classInitializations"]["sdi12"]["addr"];

  //Battery measure method
  batteryMethod = doc["classInitializations"]["battery"]["method"].as<String>();
  batteryPin = doc["classInitializations"]["battery"]["pin"];

  //RTC init
  String rtcType = doc["classInitializations"]["rtc"]["type"].as<String>();
  if (rtcType == "RTC_DS3231") {
    rtc = RTC_DS3231();
  }
  //espRtc = ESP32Time(doc["classInitializations"]["espRtc"]["offset"]);

  //Modem init
  #ifndef PPP_mode
  SerialAT.begin(115200, SERIAL_8N1, doc["classInitializations"]["gsmModem"]["rx"], doc["classInitializations"]["gsmModem"]["tx"]);
  #endif
  
  // Assign gsmConfiguration
  apn = doc["gsmConfiguration"]["apn"].as<String>();
  gprsUser = doc["gsmConfiguration"]["gprsUser"].as<String>();
  gprsPass = doc["gsmConfiguration"]["gprsPass"].as<String>();
  server = doc["gsmConfiguration"]["server"].as<String>();
  port = doc["gsmConfiguration"]["port"];
  networkRetries = doc["gsmConfiguration"]["networkRetries"];

  //Camera config
  cameraEnabled = doc["cameraConfiguration"]["enabled"];
  cameraRelay = doc["cameraConfiguration"]["relay"];
  cameraOnTime = doc["cameraConfiguration"]["onTime"];
  cameraInterval = doc["cameraConfiguration"]["interval"];


  //Additional flags
  modemNetworkEnabled = doc["systemStateFlags"]["modemNetworkEnabled"];

  // Assign Wifi Parameters
  wifiEnabled = doc["WiFiSetup"]["enabled"];
  wifiSSID = doc["WiFiSetup"]["SSID"].as<String>();
  wifiPassword = doc["WiFiSetup"]["password"].as<String>();
  wifiPort = doc["WiFiSetup"]["port"];
  wifiDurationOn = doc["WiFiSetup"]["durationOn"];
  keepWifiPoweredOn = doc["WiFiSetup"]["keepPoweredOn"];

  // Assign timingControl
  modemTurnsOffAtMinutes = doc["timingControl"]["modemTurnsOffAtMinutes"];

  Serial.println("Config loaded successfully");
}

// Function to modify the config.json file and update variables live
void modifyConfig(const char* keyPath, JsonVariant value, String reset) {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println("Failed to parse config file");
    configFile.close();
    return;
  }
  configFile.close();

  // Split keyPath into parts
  std::vector<String> keys;
  String key(keyPath);
  int index = 0;
  while ((index = key.indexOf('/')) != -1) {
    keys.push_back(key.substring(0, index));
    key.remove(0, index + 1);
  }
  keys.push_back(key);

  // Navigate to the appropriate section
  JsonVariant target = doc;
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    if (!target.containsKey(keys[i])) {
      target[keys[i]].to<JsonObject>();  // Updated to the non-deprecated method
    }
    target = target[keys[i]];
  }

  // Update or create the final key
  target[keys.back()] = value;

  // Write updated JSON to a temporary file
  File tempFile = LittleFS.open("/temp_config.json", "w");
  if (!tempFile) {
    Serial.println("Failed to open temporary config file for writing");
    return;
  }

  if (serializeJson(doc, tempFile) == 0) {
    Serial.println("Failed to write to temporary config file");
    tempFile.close();
    return;
  }
  tempFile.close();

  // Replace original config file with the updated temporary file
  LittleFS.remove("/config.json");
  LittleFS.rename("/temp_config.json", "/config.json");

  // Update the variables in the program
  if (reset == "true") {
    loadConfig();
  }

  Serial.println("Config modified and updated successfully");
}

JsonDocument readConfig() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return JsonDocument();
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return JsonDocument();
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.println("Failed to parse config file");
    return JsonDocument();
  }

  return doc;
}

