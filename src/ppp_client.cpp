#include <PPP.h>
#include <WiFi.h>
#include <Network.h>
#include "shared.h"
#include "config.h"
#include "utilities.h"
#include <freertos/projdefs.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Update.h>
#include "ppp_client.h"

#define PPP_MODEM_APN "telstra.internet"
#define PPP_MODEM_PIN ""  // or NULL

// WaveShare SIM7600 HW Flow Control
#define PPP_MODEM_TX        26
#define PPP_MODEM_RX        27
#define PPP_MODEM_FC        ESP_MODEM_FLOW_CONTROL_NONE
#define PPP_MODEM_MODEL     PPP_MODEM_SIM7600

// Enums for modem and client states
enum SimcomState {
  SPOWER_MODEM_ON,
  SPOWER_MODEM_OFF,
  SWAIT_MODEM_ON,
  SWAIT_MODEM_NETWORK,
  SMODEM_IDLE,
  SMODEM_OFF,
};

SimcomState simcomState = SPOWER_MODEM_ON;


void onEvent(arduino_event_id_t event, arduino_event_info_t info);


int network_retry_counter = 0;

void handle_ppp() {
  String response;
  bool attached;
  switch (simcomState) {
  case SMODEM_IDLE:
    return;
    break;
  case SMODEM_OFF:
  return;
    break;
  case SPOWER_MODEM_ON:
    powerModemOn();
    simcomState = SWAIT_MODEM_ON;
    return;
    break;
  case SPOWER_MODEM_OFF:
    modemOff(5);
    simcomState = SMODEM_OFF;
    simcomModemPowered = false;
    return;
    break;
    simcomState = SMODEM_OFF;
    return;
    break;
  case SWAIT_MODEM_ON:
    simcomState = SWAIT_MODEM_NETWORK;
    return;
    break;
  case SWAIT_MODEM_NETWORK:
    attached = PPP.attached();
    if (!attached) {
      int i = 0;
      unsigned int s = millis();
      Serial.print("Waiting to connect to network");
      while (!attached && ((++i) < 600)) {
        Serial.print(".");
        delay(100);
        attached = PPP.attached();
      }
      Serial.print((millis() - s) / 1000.0, 1);
      Serial.println("s");
      attached = PPP.attached();
    }
    Serial.print("Attached: ");
    Serial.println(attached);
    Serial.print("State: ");
    Serial.println(PPP.radioState());
    if (attached) {
      Serial.print("Operator: ");
      Serial.println(PPP.operatorName());
      Serial.print("IMSI: ");
      Serial.println(PPP.IMSI());
      Serial.print("RSSI: ");
      Serial.println(PPP.RSSI());
      int ber = PPP.BER();
      if (ber > 0) {
        Serial.print("BER: ");
        Serial.println(ber);
        Serial.print("NetMode: ");
        Serial.println(PPP.networkMode());
      }

      Serial.println("Switching to data mode...");
      PPP.mode(ESP_MODEM_MODE_DATA);  // Data and Command mixed mode
      if (!PPP.waitStatusBits(ESP_NETIF_CONNECTED_BIT, 1000)) {
        Serial.println("Failed to connect to internet!");
      } else {
        Serial.println("Connected to internet!");
        simcomState = SMODEM_IDLE;
      }
    } else {
      Serial.println("Failed to connect to network!");
    }
    return;
    break;
  }
}

void powerModemOn() {
  Serial.println("DEBUG: Turning modem on...");
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(300);
  digitalWrite(MODEM_PWRKEY, LOW);

  pinMode(MODEM_DTR, OUTPUT);
  digitalWrite(MODEM_DTR, LOW);

  pinMode(MODEM_FLIGHT, OUTPUT);
  digitalWrite(MODEM_FLIGHT, HIGH);
  return;
}


void PPPTaskCode(void* parameter) {
  #ifdef SERIAL_COM
  HardwareSerial SerialCom(2);
  SerialCom.begin(460800,SERIAL_8N1,26,27);
  Serial.println("Temp open serialcom");
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  SerialCom.end();
  #endif

  IPAddress ap_ip(192, 168, 4, 1);
  IPAddress ap_mask(255, 255, 255, 0);
  IPAddress ap_leaseStart(192, 168, 4, 2);
  IPAddress ap_dns(8, 8, 4, 4);

  WiFi.enableAP(true);
  WiFi.mode(WIFI_MODE_AP);
  //WiFi.mode(WIFI_MODE_APSTA);
  Serial.print("TX power");
  Serial.println(WiFi.getTxPower());
  WiFi.setTxPower(WIFI_POWER_21dBm);
  Serial.print("TX power");
  Serial.println(WiFi.getTxPower());
  // Start the Access Point
  WiFi.AP.begin();
  WiFi.AP.config(ap_ip, ap_ip, ap_mask, ap_leaseStart, ap_dns);
  WiFi.AP.create(wifiSSID.c_str(), wifiPassword.c_str());
  if (!WiFi.AP.waitStatusBits(ESP_NETIF_STARTED_BIT, 1000)) {
    Serial.println("Failed to start AP!");
  }
  xTaskNotifyGive(WiFiTask);
  // Listen for modem events
  Network.onEvent(onEvent);

  // Configure the modem
  PPP.setApn(PPP_MODEM_APN);
  PPP.setPin(PPP_MODEM_PIN);
  PPP.setPins(27, 26, ESP_MODEM_FLOW_CONTROL_NONE);

  Serial.println("Starting the modem. It might take a while!");
  PPP.begin(PPP_MODEM_SIM7600, 2, 3200000);
  PPP.setBaudrate(3200000);
  for (;;) {
    #ifdef SIMCOM_UART
    handle_ppp();
    #else
    powerModemOn();
    #endif
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

String performHttpPost(const char* server, int port, const String& path, const String& jsonData) {
  HTTPClient http;
  String payload;
  String sendData = jsonData;
  http.begin(server, port, path);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(sendData);
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  return payload;
}

String performHttpGet(const char* server, int port, const String& path) {
  HTTPClient http;
  String payload;

  // Your Domain name with URL path or IP address with path;
  http.begin(server, port, path);

  // Send HTTP GET request
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  return payload;
}

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
  case ARDUINO_EVENT_PPP_START:     Serial.println("PPP Started"); break;
  case ARDUINO_EVENT_PPP_CONNECTED: Serial.println("PPP Connected"); break;
  case ARDUINO_EVENT_PPP_GOT_IP:
    Serial.println("PPP Got IP");
    Serial.println(PPP);
    WiFi.AP.enableNAPT(true);
    break;
  case ARDUINO_EVENT_PPP_LOST_IP:
    Serial.println("PPP Lost IP");
    WiFi.AP.enableNAPT(false);
    break;
  case ARDUINO_EVENT_PPP_DISCONNECTED:
    Serial.println("PPP Disconnected");
    WiFi.AP.enableNAPT(false);
    break;
  case ARDUINO_EVENT_PPP_STOP: Serial.println("PPP Stopped"); break;

  case ARDUINO_EVENT_WIFI_AP_START:
    Serial.println("AP Started");
    Serial.println(WiFi.AP);
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:    Serial.println("AP STA Connected"); break;
  case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: Serial.println("AP STA Disconnected"); break;
  case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    Serial.print("AP STA IP Assigned: ");
    Serial.println(IPAddress(info.wifi_ap_staipassigned.ip.addr));
    break;
  case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: Serial.println("AP Probe Request Received"); break;
  case ARDUINO_EVENT_WIFI_AP_STOP:           Serial.println("AP Stopped"); break;

  default: break;
  }
}

void performOTAUpdate(const char* server, int port, const String& path) {
  Serial.println("OTA: Performing OTA update...");
  otaInProgress = true;
  
  HTTPClient http;
  http.begin("http://" + String(server) + ":" + String(port) + path);
  
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Failed to connect or get firmware: " + String(httpCode));
    http.end();
    otaInProgress = false;
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("Invalid content length, OTA aborted.");
    http.end();
    otaInProgress = false;
    return;
  }

  // Begin OTA update
  Serial.println("OTA: Initializing OTA update...");
  if (!Update.begin(contentLength)) {
    Serial.println("Failed to start OTA update.");
    http.end();
    otaInProgress = false;
    return;
  }

  // Read firmware data in chunks
  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  unsigned long timeout = millis();
  int chunkCounter = 0;

  Serial.println("OTA: Reading binary chunks...");
  while (http.connected() && written < contentLength && (millis() - timeout < 120000L)) {
    if (stream->available()) {
      uint8_t buf[512];
      int len = stream->readBytes(buf, sizeof(buf));
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

  // Complete OTA update
  if (written == contentLength && Update.end()) {
    Serial.println("OTA update successful, restarting...");
    http.end();
    ESP.restart();
  } else {
    Serial.println("OTA update failed");
    Update.abort();
  }

  http.end();
  otaInProgress = false;
}

void modemOff(int8_t retries) {
    for (int i = 0; i < retries; i++) {
      PPP.mode(ESP_MODEM_MODE_COMMAND);
      PPP.powerDown();
      delay(500);
      pinMode(MODEM_FLIGHT, OUTPUT);
      digitalWrite(MODEM_FLIGHT, LOW);
      delay(300);
      simcomModemPowered = false;
    }
  }