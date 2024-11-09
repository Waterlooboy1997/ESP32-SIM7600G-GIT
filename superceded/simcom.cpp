#include <Arduino.h>
#include "shared.h"
#include "simcom.h"
#include <ESP32Time.h>
#include <TinyGsmClient.h>
#include <Update.h>

// Access the modem and client via pointers
// GSM Modem
extern bool modemNetworkEnabled;
extern int networkRetries;
extern int port;


String sendAtReturnResponse(String cmd, unsigned long _timeout, bool okReply) {
  #ifndef PPP_mode
  String returnResult;
  unsigned long _startMillis;

  cmd = cmd + "\r\n";

  Serial.print(">> "); 
  Serial.print(cmd);   
  SerialAT.print(cmd);

  vTaskDelay(10 / portTICK_PERIOD_MS); 
  _startMillis = millis();
  do {
    returnResult += SerialAT.readString();
    if (returnResult.indexOf("OK") >= 0 && okReply) {
      _timeout = 0;
      SerialAT.flush();
    }
  } while (millis() - _startMillis < _timeout);

  Serial.println("<< ");
  returnResult.trim();
  Serial.println(returnResult);
  Serial.println("<< ");
  return returnResult;
  #else
  return "";
  #endif
}

bool modemOn() {
  #ifndef PPP_mode
  Serial.println("DEBUG: Turning modem on...");
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(300);
  digitalWrite(MODEM_PWRKEY, LOW);

  pinMode(MODEM_DTR, OUTPUT);
  digitalWrite(MODEM_DTR, LOW);

  pinMode(MODEM_FLIGHT, OUTPUT);
  digitalWrite(MODEM_FLIGHT, HIGH);

  int i = 1;
  while (i) {
    if (modem.testAT()) {
      Serial.println("MODEM: OK found, exiting loop...");
      return true;
      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    i--;
  }
  return false;
  #else
  return true;
  #endif
}

void handleModem() {
  #ifndef PPP_mode
  switch (modemState) {
    case MODEM_HANG:
      Serial.print("<mhang>");
      break;
    case MODEM_OFF:
      Serial.print("<moff>");
      break;
    case MODEM_IDLE:
      Serial.print("<midle>");
      break;
    case POWER_MODEM_ON:
      modemOn();
      modemState = WAIT_MODEM_ON;
      break;
    case POWER_MODEM_OFF:
      modemOff(1);
      modemState = MODEM_OFF;
      clientState = CLIENT_IDLE;
      simcomModemPowered = false;
      break;
    case WAIT_MODEM_ON:
      Serial.println("Waiting for modem connection...");
      if (modemOn()) {
        Serial.println("Modem on, starting network connection...");
        networkState = NETWORK_CONNECT;
        modemState = MODEM_IDLE;
      } else {
        Serial.println("Modem check failed, trying again...");
        modemState = MODEM_IDLE;
      }
      break;
  }

  switch(networkState){
    case NETWORK_HANG:
      Serial.print("<nhang>");
      break;
    case NETWORK_FAILED:
      Serial.print("<nfail>");
      networkState = NETWORK_CONNECT;
      break;
    case NETWORK_CONNECTED:
      Serial.print("<nconn>");
      break;
    case NETWORK_CONNECT:
      if (modemNetworkEnabled) {
        simCCID = modem.getSimCCID();
        Serial.println("In network connect...");
        Serial.print("Waiting for network...");
        if (!modem.waitForNetwork(60000,true)) {
          signalQuality = modem.getSignalQuality();
          Serial.println("Network wait failed...");
          networkState = NETWORK_FAILED;
        } else {
          networkState = NETWORK_CHECK;
        }
      } else {
        Serial.println("Modem network not enabled, sitting in network loop...");
        modemState = MODEM_IDLE;
      }
      break;
    case NETWORK_CHECK:
      Serial.println("In network check...");
      Serial.print("Checking for network...");
      if (!modem.isNetworkConnected()) {
        signalQuality = modem.getSignalQuality();
        Serial.println("Network check failed...");
        networkState = NETWORK_FAILED;
      } else {
        networkState = GPRS_CONNECT;
      }
      break;
    case GPRS_CONNECT:
      Serial.println("In gprs connect...");
      if (!modem.gprsConnect(apn.c_str(), gprsUser.c_str(), gprsPass.c_str())) {
        Serial.println("GPRS connect failed...");
        networkState = NETWORK_FAILED;
      } else {
        networkState = GPRS_CHECK;
      }
      break;
    case GPRS_CHECK:
      Serial.println("In gprs check...");
      if (!modem.isGprsConnected()) {
        Serial.println("GPRS check failed...");
        networkState = NETWORK_FAILED;
      } else {
        modemState = MODEM_IDLE;
        networkState = NETWORK_CONNECTED;
      }
      break;
  }
  #endif
}

void handleClient() {
  #ifndef PPP_mode
  /*
  if (clientState == CLIENT_CONNECT) {
    signalQuality = modem.getSignalQuality();
    if (!client.connect(server.c_str(), port)) {
      Serial.print('n');
    } else {
      Serial.print('y');
      clientState = CLIENT_CONNECTED;
    }
    if (!client.connected() && clientState == CLIENT_CONNECTED) {
      Serial.println("Client disconnected, forcing back on.");
      clientState = CLIENT_CONNECT;
    } else if (clientState == CLIENT_IDLE) {
      clientState = CLIENT_CONNECT;
    } else {
      Serial.print("y");
    }
    //clientState = CLIENT_CONNECT;
  } else {
    Serial.print(".");
    */
    /*
    if (client.connected()) {
      if (rowsToUpload > 0) {
        Serial.print("Uploading rows to server: ");
        Serial.println(rowsToUpload);
        const int ROWS_PER_UPLOAD = 5;

        // First, get the total number of rows in the file
        String sensorType = sensor->getSensorName();
        JsonDocument fullDoc = readJsonData(sensorType, "", 0);
        int totalRows = fullDoc["data"].size();

        int remainingRows = rowsToUpload;
        int startRow = max(0, totalRows - rowsToUpload);  // Ensure we don't go negative
        fullDoc.clear(); //clear json used for size
        vTaskDelay(2000 / portTICK_PERIOD_MS); //delay for stability

        while (remainingRows > 0) {
          int endRow = startRow + ROWS_PER_UPLOAD - 1; // Adjust endRow calculation
          if (endRow >= totalRows) {
              endRow = totalRows - 1;
          }

          if (startRow > endRow) {
              Serial.println("No more rows to process, exiting loop.");
              break; // Break the loop if startRow is beyond endRow
          }

          JsonDocument chunk = readJsonData(sensorType, "", 0, startRow, endRow);

          if (chunk.isNull() || chunk["data"].size() == 0) {
              Serial.println("No data read, skipping upload.");
              break; // Exit if no data is read to avoid sending null data
          }

          // Upload the chunk here
          // For example:
          String jsonString;
          serializeJson(chunk, jsonString);
          Serial.println("Uploading chunk: " + jsonString);
          client.println(jsonString);

          int rowsUploaded = endRow - startRow + 1;
          remainingRows -= rowsUploaded;
          rowsToUpload = remainingRows;
          startRow += ROWS_PER_UPLOAD; // Increment startRow for the next batch
          Serial.println("Sent rows to server for saving, waiting 7 seconds before next batch...");
          chunk.clear();
          jsonString = "";
          vTaskDelay(7000 / portTICK_PERIOD_MS);
        }

        rowsToUpload = 0; // Reset rowsToUpload after all rows have been processed
      }
    }
    */
   /*
  }
  */
#endif
return;
}

void modemOff(int8_t retries) {
  #ifndef PPP_mode
  for (int i = 0; i < retries; i++) {
    modem.poweroff();
    delay(500);
    pinMode(MODEM_FLIGHT, OUTPUT);
    digitalWrite(MODEM_FLIGHT, LOW);
    delay(300);
    simcomModemPowered = false;
  }
  #endif
}

void performOTAUpdate(const char* server, int port, const String& path) {
  #ifndef PPP_mode
    Serial.println("OTA: Performing OTA update...");
    otaInProgress = true;
    if (client.connect(server, port)) {
        // Send HTTP GET request for firmware
        client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                     "Host: " + server + "\r\n" +
                     "Connection: close\r\n\r\n");

        // Read and ignore headers
        bool headersPassed = false;
        int contentLength = 0;

        // Wait for the response and skip headers
        unsigned long timeout = millis();
        while (millis() - timeout < 10000L) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                if (line.startsWith("Content-Length:")) {
                    contentLength = line.substring(16).toInt();
                }
                if (line == "\r") {
                    headersPassed = true;
                    break;
                }
                timeout = millis();
            }
        }

        if (!headersPassed || contentLength <= 0) {
            Serial.println("Failed to retrieve content length or headers");
            client.stop();
            return;
        }

        // Begin OTA update
        Serial.println("OTA: Init OTA update...");
        if (!Update.begin(contentLength)) {
            Serial.println("Failed to start OTA update");
            client.stop();
            return;
        }

        // Read the binary data in chunks
        Serial.println("OTA: Reading binary chunks...");
        size_t written = 0;
        timeout = millis();
        int chunkCounter = 0;
        while (client.connected() && written < contentLength && (millis() - timeout < 120000L)) {
            if (client.available()) {
                uint8_t buf[512];
                int len = client.read(buf, sizeof(buf));
                if (len > 0) {
                    Update.write(buf, len);
                    written += len;
                    timeout = millis(); // Reset timeout whenever data is received
                }
                chunkCounter++;
                Serial.print("Chunk: ");
                Serial.println(chunkCounter);
            }
            
        }

        Serial.print("Written: ");
        Serial.print(written);
        Serial.print(" || Counted: ");
        Serial.print(chunkCounter);
        Serial.print(" || Counted * 512: ");
        Serial.print(chunkCounter * 512);
        Serial.print(" || Content len: ");
        Serial.println(contentLength);


        // End OTA update
        if (written == contentLength && Update.end()) {
            Serial.println("OTA update successful, restarting...");
            client.stop();
            ESP.restart();
        } else {
            Serial.println("OTA update failed");
            Update.abort();
            client.stop();
        }
    } else {
        Serial.println("Connection to server failed");
    }
    otaInProgress = false;
    #endif
}

String performHttpGet(const char* server, int port, const String& path) {
  #ifndef PPP_mode
  Serial.println("Performing HTTP GET request...");
  String response = "";

  if (client.connect(server, port)) {
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                 "Host: " + server + "\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    bool headersPassed = false;

    while (millis() - timeout < 10000L) {  // Wait up to 10 seconds
      if (client.connected() || client.available()) {  // Check if client is still connected or data is available
        while (client.available()) {
          String line = client.readStringUntil('\n');
          if (line == "\r") {
            headersPassed = true;
          } else if (headersPassed) {
            response += line + "\n";
          }
          timeout = millis();  // Reset timeout whenever data is received
        }
      }
    }
    client.stop();
    Serial.println("GET request completed");
  } else {
    Serial.println("Connection failed");
  }
  Serial.print("Path: ");
  Serial.print(path);
  Serial.print(" || response: ");
  Serial.println(response);
  return response;
  #else
  return "";
  #endif
}

String performHttpPost(const char* server, int port, const String& path, const String& jsonData) {
  #ifndef PPP_mode
  Serial.println("Performing HTTP POST request...");
  
  if (!client.connect(server, port)) {
    return "failed: unable to connect";
  }
  
  // Prepare the data to be sent
  //String postData = urlEncode(jsonData);
  String postData = jsonData;
  Serial.print("Posting data: ");
  Serial.println(postData);
  
  // Make an HTTP POST request
  client.print(String("POST ") + path + " HTTP/1.1\r\n" +
               "Host: " + server + "\r\n" +
               "Connection: close\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + postData.length() + "\r\n\r\n" +
               postData);

  // Wait for the response
  unsigned long timeout = millis();
  String responseStatus = "";
  bool responseReceived = false;
  
  while (client.connected() && millis() - timeout < 10000L) {
    clientState = CLIENT_CONNECTED;
    while (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line.startsWith("HTTP/1.1")) {
        responseStatus = line;
        responseReceived = true;
      }
      timeout = millis();
    }
    if (responseReceived) break;
  }
  
  client.stop();
  clientState = CLIENT_CONNECT;
  
  if (!responseReceived) {
    return "timeout: no response received";
  }
  
  Serial.println("POST request completed");
  
  if (responseStatus.indexOf("200 OK") != -1) {
    return "success";
  } else {
    return "failed: " + responseStatus;
  }
  #else
  return "";
  #endif
}

String urlEncode(const String& str) {
  #ifndef PPP_mode
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
  #else
  return "";
  #endif
}

void getSignalQuality(){
  #ifndef PPP_mode
  signalQuality = modem.getSignalQuality();
  Serial.print("Signal: ");
  Serial.println(signalQuality);
  #endif
}

void turnOnGNSS() {
  String response = sendAtReturnResponse("AT+CGPS=1,1", 300, true);
  Serial.println(response);
}

String getGNSSData() {
  return sendAtReturnResponse("AT+CGPSINFO", 300, true);
}

bool parseTimestampFromGNSS(String gnssData, struct tm& timeinfo) {
  int gnssDataIndex = gnssData.indexOf("+CGPSINFO:");
  if (gnssDataIndex != -1) {
    String data = gnssData.substring(gnssDataIndex + 11); 
    Serial.println("Extracted GNSS data: " + data);

    int fifthCommaIndex = data.indexOf(',', data.indexOf(',', data.indexOf(',', data.indexOf(',') + 1) + 1) + 1) + 1;
    int sixthCommaIndex = data.indexOf(',', fifthCommaIndex);
    int seventhCommaIndex = data.indexOf(',', sixthCommaIndex + 1);

    if (fifthCommaIndex != -1 && sixthCommaIndex != -1 && seventhCommaIndex != -1) {
      String date = data.substring(fifthCommaIndex, sixthCommaIndex);
      String time = data.substring(sixthCommaIndex + 1, seventhCommaIndex);

      Serial.println("Parsed date: " + date);
      Serial.println("Parsed time: " + time);

      if (date.length() == 6 && time.length() >= 6) {
        timeinfo.tm_mday = date.substring(0, 2).toInt();
        timeinfo.tm_mon = date.substring(2, 4).toInt() - 1;
        timeinfo.tm_year = 2000 + date.substring(4, 6).toInt() - 1900;
        timeinfo.tm_hour = time.substring(0, 2).toInt();
        timeinfo.tm_min = time.substring(2, 4).toInt();
        timeinfo.tm_sec = time.substring(4, 6).toInt();
        timeinfo.tm_isdst = -1;

        time_t rawTime = mktime(&timeinfo);
        rawTime += 10 * 3600; 
        gmtime_r(&rawTime, &timeinfo);

        Serial.print("Parsed tm structure: ");
        Serial.print(timeinfo.tm_year + 1900); Serial.print("-");
        Serial.print(timeinfo.tm_mon + 1); Serial.print("-");
        Serial.print(timeinfo.tm_mday); Serial.print(" ");
        Serial.print(timeinfo.tm_hour); Serial.print(":");
        Serial.print(timeinfo.tm_min); Serial.print(":");
        Serial.println(timeinfo.tm_sec);

        return true;
      }
    }
  }
  return false;
}

void obtainGNSSData() {
  turnOnGNSS();
  vTaskDelay(1000 / portTICK_PERIOD_MS); 
  String gnssData;
  struct tm timeinfo;
  bool gotTimestamp = false;
  const int maxIterations = 5;

  for (int i = 0; i < maxIterations && !gotTimestamp; i++) {
    gnssData = getGNSSData();
    Serial.print("GNSS Data: ");
    Serial.println(gnssData);
    gotTimestamp = parseTimestampFromGNSS(gnssData, timeinfo);
    if (gotTimestamp) {
      Serial.print("Timestamp: ");
      Serial.print(timeinfo.tm_year + 1900); Serial.print("-");
      Serial.print(timeinfo.tm_mon + 1); Serial.print("-");
      Serial.print(timeinfo.tm_mday); Serial.print(" ");
      Serial.print(timeinfo.tm_hour); Serial.print(":");
      Serial.print(timeinfo.tm_min); Serial.print(":");
      Serial.println(timeinfo.tm_sec);

      espRtc.offset = 0;
      time_t t = mktime(&timeinfo);
      struct timeval now = { .tv_sec = t };
      settimeofday(&now, NULL);
      break;
    } else {
      Serial.println("Waiting for valid GNSS data...");
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
  }

  if (!gotTimestamp) {
    Serial.println("Failed to obtain GNSS timestamp within 60 seconds.");
  }
}
