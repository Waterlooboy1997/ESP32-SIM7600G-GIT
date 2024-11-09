#include "measurements.h"
#include "utilities.h"
#include "Sensor.h"
#include "OpusSensor.h"
#include "NicoSensor.h"
#include "RTClib.h"
#include "time.h"
#include <ESP32Time.h>
#include <MCP23018.h>
#include "tasks.h"
#include "shared.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncElegantOTA.h>
#include "ppp_client.h"

// External variables not in shared.h
extern bool measurementToSave;
extern bool measurementThisScan;
extern SemaphoreHandle_t xMutex;
SoftwareSerial serialVictron(VICTRON_RX, -1);  // RX only, no TX


void mainTimingControlTaskCode(void* parameter) {
  static unsigned long lastClientCheck = 0;
  for (;;) {
    //Serial.print(F("Main high water mark: ");
    //Serial.println(uxTaskGetStackHighWaterMark(NULL));
    //checkMemory();

    TickType_t xDelay;
    int8_t minute = getMinute();

    lipoBattVolt = readBattery(BAT_ADC);

    xSemaphoreTake(xMutex, portMAX_DELAY);
    if (measurementToSave) {
      measurementToSave = false;
      lastRecord = lastRecord + 1;

      JsonDocument doc;
      JsonObject entry = doc.to<JsonObject>();

      entry["time"] = getFormattedTimestamp();
      // Create a nested array for sensor values
      JsonArray valueArray = entry["values"].to<JsonArray>();
      valueArray.add(String(lastRecord, 3));
      valueArray.add(String(battVolt, 3));
      valueArray.add(String(waterLevel, 3));

      const float* values = sensor->getValues();
      int numValues = sensor->getNumValues();

      if (values) {
        Serial.println(F("Adding values from Trios to array..."));
        for (size_t i = 0; i < numValues; i++) {
          float precisionValue = String(values[i], 3).toFloat();
          if (precisionValue > 1000) {
            precisionValue = 0;
          }
          valueArray.add(precisionValue);
        }
      } else {
        Serial.println(F("Errpr with retrieving Trios to array..."));
        Serial.print(F("Number of vals: "));
        Serial.println(numValues);
        for (size_t i = 0; i < numValues; i++) {
          valueArray.add(-999.0f);
        }
      }
      valueArray.add(String(waterTemp, 3));
      String printSerialJson;
      serializeJson(doc, printSerialJson);
      Serial.println(printSerialJson);
      String sensorName = sensor->getSensorName();
      appendToJson(sensorName, doc);
      rowsToUpload++;

      //Structure of dynamic sample intervals:
      // Check 1: Is nitrate above threshold? If so, directly change sample interval
      // Check 2: Is water level above threshold? If so, directly change sample interval
      // Check 3: Check 1 and 2 can be false and under threshold, but this check confirms whether we have a rising or falling water level

      lastNitrateReading = values[0];


      if (values[0] < nitrateThreshold) {
        sampleInterval = lowScanRate; //Less frequent sampling
      } else {
        sampleInterval = highScanRate; //More frequent sampling
      }

      if (waterLevel < waterLevelThreshold) {
        sampleInterval = lowScanRate; //Less frequent sampling
      } else {
        sampleInterval = highScanRate; //More frequent sampling
      }

      if (fabs(waterLevel - lastWaterLevel) < waterLevelChangeThreshold) {
        sampleInterval = lowScanRate; //Less frequent sampling
      } else {
        sampleInterval = highScanRate; //More frequent sampling
      }
      lastWaterLevel = waterLevel;
    }
    xSemaphoreGive(xMutex);

    if ((minute % sampleInterval) == 0 || ((minute - 1) % sampleInterval == 0) || startMeasurement) {
      if (!measurementThisScan) {
        startMeasurement = true;
        measurementThisScan = true;
        Serial.println(F("DBG: Measurement has not occured recently or has been manually written..."));
      }
    } else {
      measurementThisScan = false;
    }

    if ((minute >= 0 && minute < modemTurnsOffAtMinutes) && (modemState == MODEM_OFF)) {
      modemState = POWER_MODEM_ON;
      simcomModemPowered = true;
      clientState = CLIENT_IDLE;
    } else if ((minute > modemTurnsOffAtMinutes && minute < 60) || forceModemOff) {
      //forceModemOff = false;
      modemOff(1);
      modemState = POWER_MODEM_OFF;
      clientState = CLIENT_IDLE;
      simcomModemPowered = false;
      /*SLEEP: Check to see if logger should go/be asleep*/
    }
  #ifdef ENABLE_SLEEP_MODE
    //Delay 5 secs before going to sleep to wait for interrupt states to initialise or update
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    if (!simcomModemPowered && modemState == POWER_MODEM_OFF && !startMeasurement && !wifiOn && sleepEnabled) { /*If both modem and wifi are off and no measurements are taking place*/
      Serial.println(F("POWER: Going to sleep for 30 seconds..."));
      xDelay = 200 / portTICK_PERIOD_MS;
      vTaskDelay(xDelay);
      esp_sleep_enable_timer_wakeup(30 * 1000 * 1000);
      esp_light_sleep_start();
      Serial.println(F("POWER: Awoken from sleep..."));
      vTaskDelay(xDelay);
      /* Allow 28 seconds for other tasks to run before checking again */
    } else {
      /* Print DBG */
      if (simcomModemPowered) {
        Serial.println(F("SLEEP: Simcom powered"));
      }
      if (startMeasurement) {
        Serial.println(F("SLEEP: Meas in progress"));
      }
      if (wifiOn) {
        Serial.println(F("SLEEP: Wifi on"));
      }
      if (!sleepEnabled) {
        Serial.println(F("SLEEP: Sleep disabled"));
      }
      /* As we did not go to sleep this cycle, we want to delay this task again */;
      xDelay = 5000 / portTICK_PERIOD_MS;
    }
  #endif

    if (wifiOn && wifiEnabled) {
      if (!WiFi.isConnected()) {
        // WIFI Connection
        unsigned long currentMillis = millis();
        if (currentMillis - lastClientCheck > 30000) { // Check every 30 seconds
          lastClientCheck = currentMillis;
          if (isClientConnected()) {
            Serial.println(F("Client connected to AP"));
          } else {
            Serial.println(F("No clients connected to AP"));
          }
        }

      } else {
        Serial.println(F("Failed to connect to WiFi"));
        turnOffWiFi();
      }
    }

    xTaskNotifyGive(lightTimerTask);
    xDelay = 10000 / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
  }
}

void measureTriosTaskCode(void* parameter) {
  const TickType_t xShortDelay = pdMS_TO_TICKS(1000);  // 100ms delay between checks
  MeasureState measureState = MeasureState::IDLE;
  uint32_t measureStartTime = 0;
  int measurementRetries = 0;
  for (;;) {
    switch (measureState) {
    case MeasureState::IDLE:
      if (startMeasurement) {
        measureState = MeasureState::INIT_SENSOR;
        measurementRetries = 0;
        measureStartTime = millis();  // Reset timeout
        Serial.println(F("DBG: Starting new measurement cycle"));
      }
      break;

    case MeasureState::INIT_SENSOR:
      Serial.println(F("DBG: Turning on sensor and wiping..."));
      toggleRelay(sdi12Relay, OUTPUT);
      toggleRelay(wiperRelay, OUTPUT);
      toggleRelay(modbusRelay, OUTPUT);
      vTaskDelay(pdMS_TO_TICKS(5000));  // 5 second delay to power SDI12 and wiper
      measureSDI(); //Measure SDI12
      vTaskDelay(pdMS_TO_TICKS(5000));  // Additional 5 second relay to ensure wiper finished
      measureStartTime = millis();  // Reset timeout
      measureState = MeasureState::TRIGGER_MEASUREMENT;
      break;

    case MeasureState::TRIGGER_MEASUREMENT:
      if (millis() - measureStartTime >= 14000) {  // 9 seconds after init
        //char firmwareVersion[11];
        //uint32_t systemDateTime;
        //sensor->readFirmwareVersion(firmwareVersion) &&
        //    sensor->readSystemDateTime(systemDateTime) &&
        if (
          sensor->triggerMeasurement()) {
          Serial.println(F("DBG: Measurement successfully triggered..."));
          measureStartTime = millis();  // Reset timeout
          measureState = MeasureState::WAIT_FOR_MEASUREMENT;
        } else {
          Serial.println(F("DBG: Failed to trigger measurement, retrying..."));
          measurementRetries++;
          if (measurementRetries > 4) {
            Serial.println(F("DBG: Failed to trigger measurement after 5 attempts"));
            measureState = MeasureState::SAVE_MEASUREMENT; //We want to save the measurement regardless...
            //measureState = MeasureState::CLEANUP;
          }
        }
      }
      break;

    case MeasureState::WAIT_FOR_MEASUREMENT:
      if (millis() - measureStartTime >= 35000) {  // 30 seconds wait
        measureStartTime = millis();  // Reset timeout
        measureState = MeasureState::GET_MEASUREMENT;
      }
      break;

    case MeasureState::GET_MEASUREMENT:
      if (sensor->getMeasurement()) {
        Serial.println(F("DBG: Measurement retrieved successfully..."));
        measureState = MeasureState::SAVE_MEASUREMENT;
      } else {
        Serial.println(F("DBG: Failed to read modbus measurement..."));
        Serial.print(F("Modbus result: "));
        Serial.println(sensor->getModbusResultCode());
        //measureState = MeasureState::CLEANUP;
        measureState = MeasureState::SAVE_MEASUREMENT; //We want to save the measurement regardless...
      }
      measureStartTime = millis();  // Reset timeout
      break;

    case MeasureState::SAVE_MEASUREMENT:
      xSemaphoreTake(xMutex, portMAX_DELAY);
      measurementToSave = true;
      xSemaphoreGive(xMutex);
      measureStartTime = millis();  // Reset timeout
      measureState = MeasureState::CLEANUP;
      break;

    case MeasureState::CLEANUP:
      toggleRelay(sdi12Relay, INPUT);
      toggleRelay(wiperRelay, INPUT);
      toggleRelay(modbusRelay, INPUT);
      //mcp.setPinX(6, B, INPUT, LOW);
      startMeasurement = false;
      measureState = MeasureState::IDLE;
      Serial.println(F("DBG: Measurement cycle completed, returning to IDLE"));
      break;

    default:
      Serial.println(F("ERR: Undefined state in measureTriosTaskCode, resetting to IDLE"));
      measureState = MeasureState::IDLE;
      break;
    }

    // Check for timeout in non-IDLE states
    if (measureState != MeasureState::IDLE) {
      unsigned long stateTimeout;
      switch (measureState) {
      case MeasureState::INIT_SENSOR:
        stateTimeout = 10000;  // 10 seconds
        break;
      case MeasureState::TRIGGER_MEASUREMENT:
        stateTimeout = 60000;  // 60 seconds
        break;
      case MeasureState::WAIT_FOR_MEASUREMENT:
        stateTimeout = 35000;  // 35 seconds
        break;
      case MeasureState::GET_MEASUREMENT:
      case MeasureState::SAVE_MEASUREMENT:
        stateTimeout = 30000;  // 30 seconds
        break;
      default:
        stateTimeout = 120000;  // 2 minutes (default)
      }

      if (millis() - measureStartTime >= stateTimeout) {
        Serial.printf("ERR: Measurement cycle timed out in state %d, resetting to IDLE\n", (int)measureState);
        measureState = MeasureState::CLEANUP;
      }
    }

    vTaskDelay(xShortDelay);
  }
}

void simcomControllerTaskCode(void* parameter) {
  const long gnssUpdateFrequency = 60 * 60000; // Convert to ms
  const long clientWatch = 3000000;                                  // Five minute intervals
  unsigned long previousMillisA = 0;               // Stores the last time the event happened and starts
  unsigned long previousMillisB = 0;                                 // Stores the last time the event happened
  unsigned long previousMillisC = 0;                                 // Stores the last time the event happened
  unsigned long previousMillisD = 0;                                 // Stores the last time the event happened
  unsigned long previousMillisE = 0;                                 // Stores the last time the event happened
  unsigned long previousMillisF = 0;                                 // Stores the last time the event happened

  int8_t modemNetworkFailCounter = 0;

  bool recentCommandReceived = false;
  int32_t timestampUpdateInterval = 3600000;
  int32_t defaultCommandScanTime = 300000;
  int32_t recentCommandScanTime = 30000;
  int32_t placeholderCommandScanTime = defaultCommandScanTime;
  int8_t recentScanRequestCounter = 0;
  int8_t maxScanRequestCounter = 10;
  Serial.println(F("Starting simcom task.."));
  for (;;) {
    if (!otaInProgress) {
      unsigned long currentMillis = millis();

      //Millis check 1 -- Manually Update Timestamp Check and Automated Timestamp Updating From Server
      if ((currentMillis - previousMillisA >= timestampUpdateInterval) || manuallyUpdateTimestamp) {
        // Save the current time
        if (manuallyUpdateTimestamp) {
          manuallyUpdateTimestamp = false;
        } else {
          previousMillisA = currentMillis;
        }

        String timestampUnix = performHttpGet(server.c_str(), 3000, "/getTimestamp");
        if (timestampUnix.length() > 1) {
          int32_t timestamp = timestampUnix.toInt();
          if (timestamp > 1725021524) { //if greater than 30 aug 2024 -> checking for invalid date/timestamp
            adjustTimestamp(timestamp);
            Serial.print(F("Timestamp updated to: "));
            Serial.println(timestampUnix);
          } else {
            Serial.println(F("Timestamp invalid..."));
          }
        } else {
          Serial.println(F("Failed to get timestamp..."));
        }
      }

      if (currentMillis - previousMillisB >= 5000) {
        // Save the current time
        previousMillisB = currentMillis;
        //handleModem();
      }

      if (currentMillis - previousMillisC >= 60000) {
        // Save the current time
        previousMillisC = currentMillis;

        if (networkState == NETWORK_FAILED) {
          modemNetworkFailCounter++;
          if (modemNetworkFailCounter >= 15 && modemNetworkFailCounter <= 45) {
            forceModemOff = true;
            Serial.println(F("Network connect failed reached limit - forcing modem off for 10 minutes..."));
          } else if (modemNetworkFailCounter > 45) {
            Serial.println(F("10 minute modem network wait finished... forcing modem back on..."));
            modemNetworkFailCounter = 0;
            forceModemOff = false;
          }
          Serial.print(F("Modem network fail counter: "));
          Serial.println(modemNetworkFailCounter);
        }
      }

      if ((currentMillis - previousMillisE >= placeholderCommandScanTime) || manuallyCheckForServerCommands) {
        if (recentCommandReceived) {
          Serial.println(F("Quicker recent scan..."));
          placeholderCommandScanTime = recentCommandScanTime;
          recentScanRequestCounter++;
          if (recentScanRequestCounter > maxScanRequestCounter) {
            recentCommandReceived = false;
            recentScanRequestCounter = 0;
          }
        } else {
          Serial.println(F("Slower default scan..."));
          placeholderCommandScanTime = defaultCommandScanTime;
        }
        // Save the current time
        if (manuallyCheckForServerCommands) {
          manuallyCheckForServerCommands = false;
        } else {
          previousMillisE = currentMillis;
        }

        bool postDebugToServer = false;
        String commandResponse = performHttpGet(server.c_str(), 3000, "/api/checkCommand?id=" + loggerId);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(F("Received command from server: "));
        Serial.println(commandResponse);
        if (commandResponse.length() > 1) {
          recentCommandReceived = true;
          recentScanRequestCounter = 0;
          placeholderCommandScanTime = recentCommandScanTime;
          String trimmedCommandResponse = commandResponse;
          trimmedCommandResponse.trim();
          trimmedCommandResponse.replace("\"", "");
          if (trimmedCommandResponse == "poll") {
            postDebugToServer = true;
          }
          JsonDocument res = processCommand(commandResponse);
          if (postDebugToServer) {
            //If sending back to server to save as debug, append the logger ID for saving...
            res["id"] = loggerId;
            String jsonString;
            serializeJson(res, jsonString);
            String debugRes = performHttpPost(server.c_str(), 3000, "/api/postDebug", jsonString);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            Serial.print(F("Response from posting to debug server: "));
            Serial.println(debugRes);
            debugRes.clear();
            res.clear();
          } else {
            Serial.println(F("Skipped printing command from server to server debug..."));
          }
        } else {
          Serial.println(F("Command from server does not meet length requirements, probably no command to send..."));
        }

      }

      if ((rowsToUpload > 0 && currentMillis - previousMillisF >= 60000) || manuallyCheckTimestampForDataToUpload || currentMillis - previousMillisF >= 1200000) { //every 15ish minutes check for the last timestamp in the stored dataset and upload new data to server
        // Save the current time
        if (manuallyCheckTimestampForDataToUpload) {
          manuallyCheckTimestampForDataToUpload = false;
        } else {
          previousMillisF = currentMillis;
        }
        JsonDocument doc;
        int8_t lastDateRequestMaxRetries = 6;
        for (int8_t lastDateRequestCounter = 0; lastDateRequestCounter < lastDateRequestMaxRetries; lastDateRequestCounter++) {
          String sensorName = sensor->getSensorName();
          String lastDatasetTimestamp = performHttpGet(server.c_str(), 3000, "/api/lastDataTimestamp?id=" + loggerId + "&sensor=" + sensorName);
          lastDatasetTimestamp.trim();
          lastDatasetTimestamp.replace("\"", "");
          vTaskDelay(500 / portTICK_PERIOD_MS);
          if (lastDatasetTimestamp.length() > 5) {
            doc = readJsonData(sensorName, lastDatasetTimestamp, -1);

            if (doc["data"].size() > 1) {
              doc["id"] = loggerId;  // Add the "id" so the server knows where to save it
              doc["sensor"] = sensorName;  // Add the "id" so the server knows where to save it
              int32_t dataSize = doc["data"].size();
              Serial.print(F("Data size: "));
              Serial.println(dataSize);
              int8_t maxNumRowsToSend = 3;
              int32_t arrayPosition = 0;
              JsonDocument tempDoc;
              tempDoc["data"].to<JsonArray>();
              tempDoc["sensor"] = sensorName;
              tempDoc["id"] = loggerId;
              tempDoc["headers"] = doc["headers"];
              while (arrayPosition < dataSize) {
                for (int8_t c = 0; c < maxNumRowsToSend && arrayPosition < dataSize; c++) {
                  Serial.print(F("Adding index: "));
                  Serial.print(arrayPosition);
                  Serial.print(F(" / total of "));
                  Serial.println(dataSize);
                  tempDoc["data"].add(doc["data"][arrayPosition]);
                  arrayPosition++;
                }
                String jsonString;
                serializeJson(tempDoc, jsonString);
                Serial.print(F("Rows in temp data array: "));
                Serial.println(jsonString);
                String res = performHttpPost(server.c_str(), 3000, "/api/postData", jsonString);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                Serial.print(F("Response from posting data to server: "));
                Serial.println(res);
                tempDoc["data"].clear();
              }

              tempDoc.clear();
              rowsToUpload = 0;
            } else {
              Serial.println(F("Skipping data upload, no new data to upload from given timestamp..."));
            }
            break;
          } else {
            Serial.print(F("Failed to obtain last timestamp of data file... trying again: "));
            Serial.print(lastDateRequestCounter + 1);
            Serial.print(F("/"));
            Serial.println(lastDateRequestMaxRetries);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
          }

        }
        doc.clear();

      }
    }
    TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
  }
}

void serialReadTaskCode(void* parameter) {
  for (;;) {
    // Read data from Serial and process it
    if (Serial.available()) {
      static String incomingSerialData = "";
      while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
          if (incomingSerialData.length() > 0) {
            String JsonStringToPrint;
            serializeJson(processCommand(incomingSerialData), JsonStringToPrint);
            Serial.println(JsonStringToPrint);
            JsonStringToPrint = "";
            incomingSerialData = "";
          }
        } else {
          incomingSerialData += c;
        }
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void cameraTaskCode(void* parameter) {
  Serial.println(F("Camera task cycle started"));
  Serial.print(F("Camera enabled: "));
  Serial.println(cameraEnabled);
  Serial.print(F("Camera switch: "));
  Serial.println(cameraRelay);
  Serial.print(F("Camera on time: "));
  Serial.println(cameraOnTime);
  Serial.print(F("Camera interval: "));
  Serial.println(cameraInterval);
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(cameraInterval);

  // Initialize the xLastWakeTime variable with the current time
  xLastWakeTime = xTaskGetTickCount();
  Serial.println(F("Camera task started"));

  for (;;) {
    int8_t hour = getHour();
    Serial.print(F("Camera hour: "));
    Serial.println(hour);
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    // Check if the camera is enabled
    if (cameraEnabled) {
      if (hour > 5 && hour < 19) {
        Serial.println(F("Turning camera on"));
        // Turn on the camera
        toggleRelay(cameraRelay, OUTPUT);

        Serial.printf("Camera will remain on for %lu ms\n", cameraOnTime);
        // Keep the camera on for the specified duration
        vTaskDelay(pdMS_TO_TICKS(cameraOnTime));

        Serial.println(F("Turning camera off"));
        // Turn off the camera
        toggleRelay(cameraRelay, INPUT);
      } else {
        Serial.println(F("Night time, keeping camera powered off"));
        Serial.println(F("Turning camera off"));
        // Turn off the camera
        toggleRelay(cameraRelay, INPUT);
      }
    } else {
      Serial.println(F("Camera is disabled, skipping cycle"));
    }

    Serial.println(F("Camera task cycle completed"));
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

void WiFiTaskCode(void* parameter) {
  // Wait indefinitely for a notification to continue
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  unsigned long startAttemptTime = millis();
  // Periodically check for connected clients
  AsyncWebServer server(80);

  /* Included index.html */
  server.on("/processCommand", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebParameter* command = request->getParam(0);
    String cmd = command->value();
    Serial.print(F("Server processing command: "));
    Serial.println(cmd);
    String JsonStringToPrint;
    serializeJson(processCommand(cmd), JsonStringToPrint);
    request->send(200, "text/csv", JsonStringToPrint);
    JsonStringToPrint = "";
    });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("status.html").setCacheControl("max-age=600");
  //server.serveStatic("/", LittleFS, "/");
  // Handle OTA and other WiFi-related tasks
  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();

  // Start the WiFi timer
  if (wifiDuration > 0) {
    wifiTimer.once_ms(wifiDuration, turnOffWiFi);
  }
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void buttonTaskCode(void* parameter) {
  for (;;) {
    //Serial.print(F("button high water mark: "));
    //Serial.println(uxTaskGetStackHighWaterMark(NULL));
    TickType_t xDelay = 3000 / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);
    if (mcpInitialised) {
      bool buttonOneState = mcp.getPin(7, A);
      bool buttonTwoState = mcp.getPin(7, B);
      if (!buttonOneState) {
        startMeasurement = true;
      }
      if (!buttonTwoState) {
        Serial.println(F("Button pressed - Turning on WiFi for 5 minutes..."));
        if (!WiFi.isConnected()) {
          WiFi.mode(WIFI_MODE_APSTA); // Make sure WiFi is in the correct mode.
          WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str()); // Try connecting with SSID and password.
        }
        wifiOn = true;
        Serial.printf("Turning on WiFi for %lu milliseconds\n", 300000);
        wifiTimer.once_ms(300000, turnOffWiFi); // Schedule WiFi to turn off after duration.
      }
    }
  }
}

void lightTimerTaskCode(void* parameter) {
  for (;;) {
    // Wait indefinitely for a notification to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
  }
}

void batteryMeasureTaskCode(void* parameter) {
  if (batteryMethod == "victron") {
    serialVictron.begin(19600);
  }

  for (;;) {
    // Wait indefinitely for a notification to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (batteryMethod == "victron") {
      Serial.println(F("DBG: Starting new Victron reading cycle"));
      readVictron(600); // 5 second timeout for reading
    } else if (batteryMethod == "adc") {
      battVolt = readExternalBattery(batteryPin);
      Serial.print(F("Battery voltage: "));
      Serial.println(battVolt);
    } else {
      Serial.println(F("No battery method selected..."));
    }
  }
}