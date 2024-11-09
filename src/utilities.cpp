#include "utilities.h"
#include "LittleFS.h"
#include "FileWriter.h"
#include "Sensor.h"
#include "OpusSensor.h"
#include "NicoSensor.h"
#include "shared.h"
#include <ESP32Time.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <WString.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"
#include "measurements.h"
#include "utilities.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <IPAddress.h>
#include "esp_partition.h"
#include "ppp_client.h"

bool manuallyResetJson = false;

ArduinoJson::V710PB22::JsonDocument processCommand(String command) {
  command.trim();
  command.replace("\"", "");

  Serial.print(F("Processing Command: "));
  Serial.println(command);
  JsonDocument responseDoc;

  // Always include the command in the response
  responseDoc["command"] = command;

  if (!atMode) {
    if (command.startsWith("trios")) {
      String subCommand = command.substring(5);
      responseDoc["type"] = "sensor";
      if (subCommand == "getfirmware") {
        char firmwareVersion[11];
        if (sensor->readFirmwareVersion(firmwareVersion)) {
          responseDoc["data"]["firmwareVersion"] = firmwareVersion;
        } else {
          responseDoc["type"] = "error";
          responseDoc["message"] = "Failed to read firmware version";
        }
      } else if (subCommand == "gettimestamp") {
        uint32_t systemDateTime;
        if (sensor->readSystemDateTime(systemDateTime)) {
          responseDoc["data"]["systemDateTime"] = systemDateTime;
        } else {
          responseDoc["type"] = "error";
          responseDoc["message"] = "Failed to read system date and time";
        }
      } else if (subCommand == "getname") {
        responseDoc["data"]["sensorName"] = sensor->getSensorName();
      } else if (subCommand == "setmeasurement") {
        startMeasurement = true;
        responseDoc["type"] = "configuration";
        responseDoc["message"] = "Measurement triggered";
      } else if (subCommand == "getmeasurement") {
        if (sensor->getMeasurement()) {
          responseDoc["type"] = "measurement";
          responseDoc["data"]["measurement"] = sensor->getMeasurementString();
        } else {
          responseDoc["type"] = "error";
          responseDoc["message"] = "Failed to get measurement";
        }
      } else {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Unknown Trios command: " + subCommand;
      }
    } else if (command.startsWith("setotamode")) {
      String param = command.substring(10);
      responseDoc["type"] = "configuration";
      if (param == "on") {
        otaMode = true;
        otaInitialized = false;
        responseDoc["message"] = "Entering OTA mode";
      } else if (param == "off") {
        otaMode = false;
        otaInitialized = false;
        responseDoc["message"] = "Exiting OTA mode";
      } else {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Invalid parameter for setOTAMode: " + param;
      }
    } else if (command.startsWith("setatmode")) {
      String param = command.substring(9);
      responseDoc["type"] = "configuration";
      if (param == "on") {
        atMode = true;
        responseDoc["message"] = "Entering AT mode";
      } else if (param == "off") {
        atMode = false;
        responseDoc["message"] = "Exiting AT mode";
      } else {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Invalid parameter for setATMode: " + param;
      }
    } else if (command == "poll") {
      responseDoc["type"] = "diagnostic";

      JsonObject data = responseDoc["data"].to<JsonObject>();
      JsonDocument pollData = returnJsonParameters();
      for (JsonPair kv : pollData.as<JsonObject>()) {
        data[kv.key()] = kv.value();
      }
    } else if (command.startsWith("setsensortype")) {
      String sensorType = command.substring(13);
      responseDoc["type"] = "configuration";
      if (sensorType == "nico") {
        SENSOR_TYPE = SENSOR_TYPE_NICO;
      } else if (sensorType == "opus") {
        SENSOR_TYPE = SENSOR_TYPE_OPUS;
      } else {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Invalid sensor type: " + sensorType;
      }
      initModbus(false);
      responseDoc["data"]["sensorType"] = sensor->getSensorName();
    } else if (command == "setmeasurementcycle") {
      startMeasurement = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Measurement cycle triggered";
    } else if (command.startsWith("settimestamp")) {
      String param = command.substring(12);
      responseDoc["type"] = "configuration";
      if (param.length() == 0) {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Missing parameter for setTimestamp";
      } else {
        long timestamp = param.toInt();
        if (timestamp == 0) {
          responseDoc["type"] = "error";
          responseDoc["message"] = "Invalid timestamp: " + param;
        } else {
          // Update ESP32 RTC
          adjustTimestamp(timestamp);
          responseDoc["message"] = "Timestamp updated to " + String(timestamp);
        }
      }
    } else if (command == "reset") {
      responseDoc["type"] = "system";
      responseDoc["message"] = "Resetting...";
      ESP.restart();
    } else if (command.startsWith("setmodem")) {
      String param = command.substring(8);
      responseDoc["type"] = "configuration";
      if (param == "on") {
        modemTurnsOffAtMinutes = 59;
        modemState = POWER_MODEM_ON;
        responseDoc["message"] = "Modem set to stay on";
      } else if (param == "off") {
        modemTurnsOffAtMinutes = 2;
        modemState = POWER_MODEM_OFF;
        responseDoc["message"] = "Modem set to turn off";
      } else {
        responseDoc["type"] = "error";
        responseDoc["message"] = "Invalid parameter for setModem: " + param;
      }
    } else if (command == "getgnss") {
      //obtainGNSSData();
      responseDoc["type"] = "system";
      responseDoc["message"] = "GNSS data obtained";
    } else if (command == "getmodemstate") {
      responseDoc["type"] = "diagnostic";
      responseDoc["data"]["modemState"] = modemState;
    } else if (command == "getclientstate") {
      responseDoc["type"] = "diagnostic";
      responseDoc["data"]["clientState"] = clientState;
    } else if (command == "getmodbusresultcode") {
      responseDoc["type"] = "diagnostic";
      responseDoc["data"]["modbusResultCode"] = sensor->getModbusResultCode();
    } else if (command == "measuresdi") {
      measureSDI();
      responseDoc["type"] = "measurement";
      responseDoc["message"] = "SDI measurements taken";
    } else if (command == "setmcpoutput") {
      mcp.setPinX(6, B, OUTPUT, LOW);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "MCP port 6B set OUTPUT (ON)";
    } else if (command == "setmcpinput") {
      mcp.setPinX(6, B, INPUT, LOW);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "MCP port 6B set INPUT (OFF)";
    } else if (command == "jsonformat") {
      manuallyResetJson = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Forcing json format";
    } else if (command == "savemeas") {
      measurementToSave = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Saving last meas";
    } else if (command.startsWith("printjson=")) {


      // Print JSON data based on parameters
      String params = command.substring(command.indexOf('=') + 1);
      int firstCommaIndex = params.indexOf(',');
      int secondCommaIndex = params.indexOf(',', firstCommaIndex + 1);

      String sensorType = params.substring(0, firstCommaIndex);  // Sensor type, e.g., "NICO"
      String timestamp = (secondCommaIndex != -1) ? params.substring(firstCommaIndex + 1, secondCommaIndex) : params.substring(firstCommaIndex + 1);
      int lastNRows = (secondCommaIndex != -1) ? params.substring(secondCommaIndex + 1).toInt() : -1;

      if (timestamp == "\"\"") {
        timestamp = "";
      }

      responseDoc = readJsonData(sensorType, timestamp, lastNRows);
      responseDoc["type"] = "measurement";  // Add the "type" parameter as "measurement"

    } else if (command.startsWith("updatejson=")) {
      responseDoc["type"] = "configuration";
      // Extract the parameters from the command
      String params = command.substring(command.indexOf('=') + 1);
      int firstCommaIndex = params.indexOf(',');
      String keyPath = params.substring(0, firstCommaIndex);
      String valueString = params.substring(firstCommaIndex + 1);
      String requiresReset = params.substring(firstCommaIndex + 2);

      // Parse the valueString to determine the type of the value
      JsonDocument doc;
      deserializeJson(doc, "{}");
      JsonVariant value = doc.as<JsonVariant>();

      // Attempt to parse valueString as an integer
      bool isInt = true;
      for (char c : valueString) {
        if (!isdigit(c) && c != '-') {
          isInt = false;
          break;
        }
      }
      if (isInt) {
        value.set(valueString.toInt());
      } else if (valueString == "true" || valueString == "false") {
        // Attempt to parse valueString as a boolean
        value.set(valueString == "true");
      } else {
        // Otherwise, treat it as a string
        value.set(valueString);
      }

      // Call modifyConfig with the parsed keyPath and value
      modifyConfig(keyPath.c_str(), value, requiresReset);
      Serial.println(F("Config updated successfully"));
    } else if (command == "getconfig") {
      responseDoc["type"] = "configuration";
      responseDoc["data"] = readConfig();
    } else if (command == "setrowsupload") {
      responseDoc["type"] = "configuration";
      rowsToUpload = 30;
      responseDoc["message"] = "Upload rows updated to 30";
    } else if (command == "testpost") {
      /*
      if (clientState == CLIENT_CONNECT || clientState == CLIENT_IDLE) {
        JsonDocument doc;
        doc = readJsonData("NICO", "", 1);
        doc["sensor"] = "NICO";  // Add the "type" parameter as "measurement"
        doc["id"] = loggerId;  // Add the "type" parameter as "measurement"
        String jsonString;
        serializeJson(doc, jsonString);
        String res = performHttpPost(server.c_str(), 3000, "/api/postData", jsonString);
        responseDoc["type"] = "configuration";
        responseDoc["message"] = "TinyGSMClient post sent...\r\n" + jsonString + "\r\nRes: " + res;
      } else {
        responseDoc["type"] = "configuration";
        responseDoc["message"] = "TinyGSMClient not connected, skipping POST...";
      }
      */
      manuallyCheckTimestampForDataToUpload = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Manual Check Timestamp set to true";
    } else if (command == "testcheckcommand") {
      /*
      if (clientState == CLIENT_CONNECT) {
        String commandResponse = performHttpGet(server.c_str(), 3000, "/api/checkCommand?id=" + loggerId);
        responseDoc["id"] = loggerId;
        responseDoc["type"] = "configuration";
        responseDoc["message"] = "Received command: " + commandResponse;
        Serial.print(F("Received command from server: ");
        Serial.println(commandResponse);
        processCommand(commandResponse);
      } else {
        responseDoc["type"] = "configuration";
        responseDoc["message"] = "TinyGSMClient not connected, skipping GET...";
      }
      */
      manuallyCheckForServerCommands = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Manual Check Command set to true";
    } else if (command == "forcemodemoff") {
      forceModemOff = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Forcing modem off";
    } else if (command == "forcemodemon") {
      forceModemOff = false;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Forcing modem on";
    } else if (command == "readmodemstatuspin") {
      pinMode(MODEM_STATUS, INPUT);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Modem Status: " + digitalRead(MODEM_STATUS);
    } else if (command == "dtrlow") {
      pinMode(MODEM_DTR, OUTPUT);
      digitalWrite(MODEM_DTR, LOW);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Modem DTR Set Low";
    } else if (command == "dtrhigh") {
      pinMode(MODEM_DTR, OUTPUT);
      digitalWrite(MODEM_DTR, HIGH);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Modem DTR Set High";
    } else if (command == "forcemodemcycleon") {
      modemState = POWER_MODEM_ON;
      //simcomModemPowered = true;
      // = CLIENT_IDLE;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "POWER_MODEM_ON";
    } else if (command == "forcemodemcycleoff") {
      modemState = POWER_MODEM_OFF;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "POWER_MODEM_OFF";
    } else if (command == "restartneworkcycle") {
      networkState = NETWORK_CONNECT;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "NETWORK_CONNECT";
    } else if (command == "resetbatterymonitor"){
      firstVictronMeasurement = false;
      
      responseDoc["type"] = "diagnostic";
      responseDoc["message"] = "Battery monitoring reset";
    } else if (command == "resetopusfile"){
      String jsonString = "{\"sensor\":\"OPUS\",\"id\":310,\"headers\":[\"Rec\",\"Battery Voltage (v)\",\"Water Level (m)\",\"Nitrate-nitrogen (mg/L)\",\"TSS\",\"SQI\",\"ABS360\",\"ABS210\",\"ABS254\",\"UVT254\",\"SAC254\",\"Water Temp (degC)\"],\"data\":[]}";
      resetJsonFile("OPUS", jsonString);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Reset the OPUS json file";
    } else if (command == "resetnicofile"){
      String jsonString = "{\"sensor\": \"NICO\",\"headers\":[\"Rec\",\"Battery Voltage (v)\",\"Water Level (m)\",\"Nitrate-nitrogen (mg/L)\",\"SQI\",\"REFA\",\"REFB\",\"REFC\",\"REFD\",\"Water Temp (degC)\"],\"data\":[]}";
      resetJsonFile("NICO", jsonString);
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Reset the NICO json file";
    } else if (command == "updatetimestampserver"){
      manuallyUpdateTimestamp = true;
      responseDoc["type"] = "configuration";
      responseDoc["message"] = "Manually updating timestamp from server";
    } else if (command == "ismodemforcedoff"){
      responseDoc["type"] = "configuration";
      if(forceModemOff){
        responseDoc["message"] = "Modem forced off";
      }
    } else if (command == "otafirmware"){
      responseDoc["type"] = "ota";
      responseDoc["message"] = "OTA firmware update received...";
      performOTAUpdate(server.c_str(), 3000, "/firmware/firmware.bin");
    } else if (command == "printpartitions"){
      responseDoc["type"] = "partitions";
      responseDoc["message"] = "printing partitions";
      printPartitions();
    } else if (command == "testppphttp"){
      responseDoc["type"] = "ppp";
      responseDoc["message"] = "testing httpget ";
      String timestampUnix = performHttpGet(server.c_str(), 3000, "/getTimestamp");
      responseDoc["message"] = timestampUnix;
    } else {
      responseDoc["type"] = "error";
      responseDoc["message"] = "Unknown command received: " + String(command);
    }
  } else {
    if (command == "setAtModeOff") {
      atMode = false;https://www.googleadservices.com/pagead/aclk?sa=L&ai=DChcSEwi32oD1s4yIAxVno2YCHT2oNfkYABAUGgJzbQ&co=1&ase=2&gclid=CjwKCAjw5qC2BhB8EiwAvqa41urY_WZ5O7JHJdAg-x5I0_QltvmBu6shQ8GOUPaMvDEfTFWu2D8puRoCk0QQAvD_BwE&ohost=www.google.com&cid=CAESVuD2VkA4nMXrXfQu9xzjzQQ93xyaejjlDbAbSxuHJc8-dcwI9dcCyv1l2gNvhjUR5lg7EOCtxY7LmZ_ABxqDYckUwNRyJkQY6nqN0szWJkE6T3jeaxNr&sig=AOD64_1UVe4OWKVe4OZdGrttnBPkyL6AgQ&ctype=5&q=&nis=4&ved=2ahUKEwivt_v0s4yIAxXH3jgGHbJDDAQQ9aACKAB6BAgFEBc&adurl=
    responseDoc["type"] = "configuration";
    responseDoc["message"] = "Exiting AT mode";
    } else {
      responseDoc["type"] = "at";
      //sendAtReturnResponse(command, 1000, true);
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
  }

  return responseDoc;
}

ArduinoJson::V710PB22::JsonDocument readJsonData(const String& sensorType, const String& startTime, int lastNRows, int startRow, int endRow) {
    printHeapInfo("Starting readJson function");
    JsonDocument finalDoc;
    finalDoc["sensorType"] = sensorType;  // Include the sensor type in the JSON output
    finalDoc["data"].to<JsonArray>();

    Serial.println(F("Starting readJsonData function"));
    Serial.printf("Sensor Type: %s, Start Time: %s, Last N Rows: %d, Start Row: %d, End Row: %d\n",
                  sensorType.c_str(), startTime.c_str(), lastNRows, startRow, endRow);

    String filePath = "/" + sensorType + ".json";
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Serial.println(F("Failed to open file for reading"));
        return finalDoc;
    }

    // Find and copy the headers
    if (!file.find("\"headers\":[") && !file.find("\"headers\": [")) {
        Serial.println(F("Failed to find headers"));
        file.close();
        return finalDoc;
    }

    
    // Manually read the headers array
    String headersContent = "[" + file.readStringUntil(']') + "]";

    // Print the extracted headers for debugging
    Serial.print(F("headers:"));
    Serial.println(headersContent);

    // Create a valid JSON object containing only the headers
    String validHeadersJson = "{\"headers\":" + headersContent + "}";

    // Temporarily deserialize just the headers array
    JsonDocument tempDoc;  // Allocate enough memory to hold the headers
    DeserializationError error = deserializeJson(tempDoc, validHeadersJson);
    if (error) {
        Serial.printf("Failed to parse headers, error: %s\n", error.c_str());
        file.close();
        return finalDoc;
    }

    // Copy headers to the final document
    finalDoc["headers"] = tempDoc["headers"];


    // Find the start of the data array
    if (!file.find("\"data\":[") && !file.find("\"data\": [")) {
        Serial.println(F("Failed to find the start of the \"data\" array"));
        file.close();
        return finalDoc;
    }

    // First pass: Count total rows
    int totalRows = 0;
    while (file.available()) {
        JsonDocument rowDoc;  // Adjust size based on expected row size
        error = deserializeJson(rowDoc, file);
        if (error) {
            if (error == DeserializationError::EmptyInput) {
                break;  // Reached end of array
            }
            continue;
        }
        totalRows++;

        // Skip past the comma or end of array
        char c = file.peek();
        if (c == ',') {
            file.read();
        } else if (c == ']') {
            break;
        }
    }

    // Calculate starting row for lastNRows
    int startNRows = (lastNRows > 0) ? max(totalRows - lastNRows, 0) : startRow;

    // Reset file pointer to start of data array
    file.seek(0);
    if (!file.find("\"data\":[") && !file.find("\"data\": [")) {
        Serial.println(F("Failed to find the start of the \"data\" array after reset"));
        file.close();
        return finalDoc;
    }

    // Second pass: Extract the relevant rows
    int count = 0;
    int matchedRows = 0;
    int failedRows = 0;

    while (file.available()) {
        JsonDocument rowDoc;  // Adjust size as needed
        error = deserializeJson(rowDoc, file);
        // Move past the comma if there's another object in the array
        char c = file.peek();
        if (c == ',') {
            file.read();  // Move the file pointer past the comma
        } 
        if (error) {
            if (error == DeserializationError::EmptyInput) {
                break;  // Reached end of array
            }

            failedRows++;
            Serial.printf("Failed to parse row, error: %s", error.c_str());
            Serial.print(F("Total failed: "));
            Serial.print(failedRows);
            Serial.print(F(" at count: "));
            Serial.println(count + 1); //add row because uses last count
            continue;
        }

        // Skip rows until we reach the desired start
        if (count < startNRows) {
            count++;
            continue;
        }

        JsonObject obj = rowDoc.as<JsonObject>();
        count++;

        // Apply filtering conditions
        if (endRow != -1 && count > endRow) break;

        bool addObject = true;  // Default to true when no filters are applied

        // If a startTime is provided, apply the timestamp filter
        if (startTime.length() > 0 && obj.containsKey("time") && obj["time"].as<String>().length() > 0) {
            time_t startTimestamp = parseTimestamp(startTime.c_str());
            time_t objTimestamp = parseTimestamp(obj["time"]);
            addObject = (objTimestamp >= startTimestamp);
        }

        if (addObject) {
            finalDoc["data"].add(obj);
            matchedRows++;
            // Stop once we have the lastNRows
            if (lastNRows > 0 && matchedRows >= lastNRows) break;
        }

    }
    file.close();
    Serial.printf("Finished processing data. Returned %d rows.\n", matchedRows);
    printHeapInfo("Ending readJson function");
    return finalDoc;
}

ArduinoJson::V710PB22::JsonDocument returnJsonParameters() {
  JsonDocument finalDoc;
  //Trigger victron battery task when polling
  xTaskNotifyGive(batteryMeasureTask);
  finalDoc["espts"] = espRtc.getTime("%F %T"); //Timestamp
  finalDoc["rtcts"] = getFormattedTimestamp(); // Timestamp
  finalDoc["rtcinit"] = rtcInitialised ? true : false; //Real-time-clock Initialised:
  finalDoc["espRtcInit"] = espRtcInitialised ? true : false; //ESP32-clock Initialised:
  finalDoc["mcpInit"] = mcpInitialised ? true : false; //IO-expander Initialised:
  finalDoc["meas"] = startMeasurement ? true : false; //Measurement state:
  finalDoc["simPowered"] = simcomModemPowered ? true : false; //Modem power state:
  finalDoc["clientS"] = clientState; //Modem client state:
  finalDoc["modemS"] = modemState; //Modem cycle state:
  finalDoc["networkS"] = networkState; //Modem cycle state:
  finalDoc["ccid"] = simCCID; //Modem sim CCID: 
  finalDoc["signal"] = signalQuality; //Modem signal quality: 
  finalDoc["ipAddr"] = localIP; //Modem signal quality: 
  finalDoc["draw"] = currentDraw; //Current Draw (mA):
  finalDoc["fsFill"] = getLittleFsFillPercent(); //Filesystem Fill (%):
  finalDoc["battV"] = battVolt; //External Battery Voltage (v):
  finalDoc["lipo"] = lipoBattVolt; //18650 Battery Voltage (mV):
  finalDoc["rows"] = rowsToUpload; //Rows To Upload:
  finalDoc["sampleInt"] = sampleInterval; //Rows To Upload:
  return finalDoc;
}


void checkMemory() {
  size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  Serial.print(F("Free heap: "));
  Serial.println(freeHeap);
}

// Function to set the time from compile date and time
void setTimeFromCompile() {
  // __DATE__ format: "Mmm dd yyyy"
  // __TIME__ format: "hh:mm:ss"
  const char* compile_date = __DATE__;
  const char* compile_time = __TIME__;

  struct tm t;
  // Parse compile date
  char month[4];
  int day, year;
  sscanf(compile_date, "%s %d %d", month, &day, &year);

  // Convert month string to int
  if (strcmp(month, "Jan") == 0) t.tm_mon = 0;
  else if (strcmp(month, "Feb") == 0) t.tm_mon = 1;
  else if (strcmp(month, "Mar") == 0) t.tm_mon = 2;
  else if (strcmp(month, "Apr") == 0) t.tm_mon = 3;
  else if (strcmp(month, "May") == 0) t.tm_mon = 4;
  else if (strcmp(month, "Jun") == 0) t.tm_mon = 5;
  else if (strcmp(month, "Jul") == 0) t.tm_mon = 6;
  else if (strcmp(month, "Aug") == 0) t.tm_mon = 7;
  else if (strcmp(month, "Sep") == 0) t.tm_mon = 8;
  else if (strcmp(month, "Oct") == 0) t.tm_mon = 9;
  else if (strcmp(month, "Nov") == 0) t.tm_mon = 10;
  else if (strcmp(month, "Dec") == 0) t.tm_mon = 11;

  t.tm_mday = day;
  t.tm_year = year - 1900;

  // Parse compile time
  sscanf(compile_time, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
  t.tm_isdst = -1;

  // Convert to time_t (Unix timestamp)
  time_t compileUnixTime = mktime(&t);

  // Set the system time
  struct timeval tv;
  tv.tv_sec = compileUnixTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

// Function to parse timestamp string to time_t
time_t parseTimestamp(const char* timestamp) {
    struct tm tm = { 0 };
    char* result = strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm);
    /*
    if (result == NULL) {
        Serial.println("strptime failed to parse the timestamp");
    } else {
        Serial.printf("Parsed time: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    */
    time_t timeValue = mktime(&tm);
    if (timeValue == -1) {
        Serial.println(F("mktime failed to convert the struct tm to time_t"));
    }

    return timeValue;
}


void resetJsonFile(const String& filename, const String& jsonString) {
  printHeapInfo("Starting resetJsonFile function");
  Serial.print(F("Filename: "));
  Serial.println(filename);

  if (!LittleFS.begin()) {
    Serial.println(F("Failed to mount LittleFS"));
    return;
  }

  String filePath = "/" + filename + ".json";

  // Remove the existing file if it exists
  if (LittleFS.exists(filePath)) {
    Serial.println(F("File exists, deleting..."));
    LittleFS.remove(filePath);
  }

  // Create a new file with the supplied JSON string
  File file = LittleFS.open(filePath, "w");
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Write the supplied JSON string to the file
  file.print(jsonString);

  file.close();
  Serial.println(F("File reset with supplied JSON string."));
  printHeapInfo("Ending resetJsonFile function");
}


void appendToJson(const String& filename, const JsonDocument& entry) {
  printHeapInfo("Starting appendToJson function");
  Serial.print(F("Filename: "));
  Serial.println(filename);

  if (!LittleFS.begin()) {
    Serial.println(F("Failed to mount LittleFS"));
    return;
  }

  String filePath = "/" + filename + ".json";
  File file = LittleFS.open(filePath, "r+");

  if (!file) {
    Serial.println(F("Failed to open file, creating new file"));
    file = LittleFS.open(filePath, "w+");
    if (!file) {
      Serial.println(F("Failed to create file"));
      return;
    }
    file.print("{\"headers\":[],\"data\":[]}");
    file.seek(0);
  }

  // Search for the "data" array start position
  if (!file.find("\"data\":[") && !file.find("\"data\": [")) {
    Serial.println(F("Failed to find the start of the \"data\" array"));
    file.close();
    return;
  }

  // Record the position of the start of the array
  int dataIndex = file.position() - 1;

  // Search for the end of the "data" array
  char c;
  int bracketLevel = 1;  // We start within the "data" array
  int endDataIndex = -1;

  while (file.available()) {
    c = file.read();
    if (c == '[') bracketLevel++;
    if (c == ']') bracketLevel--;

    if (bracketLevel == 0) {
      endDataIndex = file.position() - 1;
      break;
    }
  }

  if (endDataIndex == -1) {
    Serial.println(F("Failed to find the end of the \"data\" array"));
    file.close();
    return;
  }

  // Determine if the array is empty or not
  file.seek(endDataIndex - 1);
  char lastChar;
  file.readBytes(&lastChar, 1);
  file.seek(endDataIndex);  // Return to the end position for appending

  if (lastChar != '[') {
    file.print(",");  // Add a comma if the array already has elements
  }

  // Append the new entry directly to the file
  serializeJson(entry, file);

  // Close the JSON structure
  file.print("]}");

  file.close();
  printHeapInfo("Ending appendToJson function");
}


float getLittleFsFillPercent() {
  if (!LittleFS.begin()) {
    Serial.println(F("An error has occurred while mounting LittleFS"));
    return 0;
  }
  // Get total and used bytes
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();

  // Calculate filled percentage
  float filledPercentage = (float)usedBytes / totalBytes * 100;

  // Print the filled percentage
  Serial.print(F("LittleFS Filled: "));
  Serial.print(filledPercentage);
  Serial.println(F("%"));
  return filledPercentage;
}

void turnOffWiFi() {
  if (!isClientConnected() && !keepWifiPoweredOn) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiOn = false;
    Serial.println(F("WiFi turned off"));
  } else {
    Serial.println(F("Client still connected or set to always on.. WiFi remains on"));
    // Optionally, you can reschedule the check
    wifiTimer.once(60, turnOffWiFi); // Check again in 60 seconds
  }
}

void toggleRelay(int8_t relayNumber, uint8_t state) {
  Serial.print(F("Setting relay: "));
  Serial.print(relayNumber);
  Serial.print(F(" to: "));
  Serial.println(state);

  if (relayNumber == 1) {
    mcp.setPinX(6, A, state, LOW);
    mcp.setPinX(6, B, state, LOW);
  } else if (relayNumber == 2) {
    mcp.setPinX(5, A, state, LOW);
    mcp.setPinX(5, B, state, LOW);
  } else if (relayNumber == 2) {
    mcp.setPinX(4, A, state, LOW);
    mcp.setPinX(4, B, state, LOW);
  } else if (relayNumber == 2) {
    mcp.setPinX(3, A, state, LOW);
    mcp.setPinX(3, B, state, LOW);
  }
}

// Function to turn on WiFi for a specified duration
void turnOnWiFiForDuration(unsigned long duration) {
  wifiDuration = duration;
  wifiOn = true;
  Serial.printf("Turning on WiFi for %lu milliseconds\n", duration);
}

bool isClientConnected() {
  // Check if any clients are connected to the SoftAP
  return WiFi.softAPgetStationNum() > 0;
}

void printHeapInfo(const char* message) {
  Serial.printf("%s - Free Heap: %d, Largest Free Block: %d\n",
    message,
    ESP.getFreeHeap(),
    ESP.getMaxAllocHeap());
}

int8_t getHour() {
    if (rtcInitialised) {
        return rtc.now().hour();
    } else {
        return espRtc.getHour(); // Replace with the actual method for espRtc
    }
}

int8_t getMinute() {
    if (rtcInitialised) {
        return rtc.now().minute();
    } else {
        return espRtc.getMinute(); // Replace with the actual method for espRtc
    }
}

String getFormattedTimestamp() {
    char buffer[20];
    if (rtcInitialised) {
        DateTime now = rtc.now();
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
    } else {
        // Assuming these methods exist in espRtc
        int year = espRtc.getYear();
        int month = espRtc.getMonth();
        int day = espRtc.getDay();
        int hour = espRtc.getHour();
        int minute = espRtc.getMinute();
        int second = espRtc.getSecond();
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                 year, month, day, hour, minute, second);
    }
    return String(buffer);
}

void adjustTimestamp(uint32_t timestamp) {
    if (timestamp > 1725021524) {  // Valid timestamp check
        if (rtcInitialised) {
            rtc.adjust(DateTime(timestamp));
        }
        espRtc.setTime(timestamp);  // Assuming `setTime()` exists on `espRtc`
        Serial.print(F("Timestamp updated to: "));
        Serial.println(timestamp);
    } else {
        Serial.println(F("Invalid timestamp, not adjusted."));
    }
}

void printPartitions() {
    Serial.println("Partitions:");
    const esp_partition_t *partition = NULL;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

    while (it != NULL) {
        partition = esp_partition_get(it);
        Serial.printf("Name: %s, Type: %d, Subtype: %d, Address: 0x%08x, Size: %d\n",
                      partition->label,
                      partition->type,
                      partition->subtype,
                      partition->address,
                      partition->size);
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
}