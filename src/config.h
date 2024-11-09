#include <ArduinoJson.h>

#ifndef CONFIG_H
#define CONFIG_H

void initMCP();
void initModbus(bool testCommunication);
void initRTC();
void loadConfig();
void modifyConfig(const char* keyPath, JsonVariant value, String reset);
JsonDocument readConfig();

#endif // CONFIG_H
