#include <freertos/portmacro.h>
#include <cstdint>

#ifndef PPP_CLIENT_H
#define PPP_CLIENT_H

void powerModemOn();
void PPPTaskCode(void* parameter);
String performHttpGet(const char* server, int port, const String& path);
String performHttpPost(const char* server, int port, const String& path, const String& jsonData);
void modemOff(int8_t retries);
void performOTAUpdate(const char* server, int port, const String& path);
void handle_ppp();

#endif // CONFIG_H
