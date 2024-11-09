#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <cstdint>
#include <esp32-hal-adc.h>
#include <esp32-sdi12.h>

// External declarations for SDI-12 Communication and other variables
extern ESP32_SDI12 sdi12;
extern SoftwareSerial serialVictron;

// Measurement variables
extern float waterLevel;
extern float waterTemp;
extern float lipoBattVolt;
extern float battVolt;
extern float current;
extern float values[2];

float readBattery(int8_t pin);
float readExternalBattery(int8_t pin);
void readVictron(unsigned long _timeout);
float readSBLT(int8_t pin);
void measureSDI();
void readVictron(unsigned long _timeout);

#endif // MEASUREMENTS_H