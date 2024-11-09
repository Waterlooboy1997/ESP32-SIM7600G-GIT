#ifndef SIMCOM_H
#define SIMCOM_H
#include <map>
#include <Arduino.h>// Define a struct for holding HTTP request details
struct HttpRequest {
  String method;
  String path;
  std::map<String, String> headers;
};
String sendAtReturnResponse(String cmd, unsigned long _timeout, bool okReply);
void turnOnGNSS();
bool parseTimestampFromGNSS(String gnssData, struct tm& timeinfo);
void obtainGNSSData();
String getGNSSData();
bool modemOn();
void modemOff(int8_t retries);
void handleModem();
void handleClient();
void readSimcomClient();
void resetClient();
void getSignalQuality();
String urlEncode(const String& str);
String performHttpPost(const char* server, int port, const String& path, const String& jsonData);
String performHttpGet(const char* server, int port, const String& path);
void performOTAUpdate(const char* server, int port, const String& path);
extern TaskHandle_t batteryMeasureTask;
#endif // SIMCOM_H