#include <Arduino.h>
#include "measurements.h"
#include "utilities.h"  // Include additional headers needed
#include "shared.h"

float readBattery(int8_t pin) {
  int vref = 1100;
  int16_t adc = analogRead(pin);
  float battery_voltage = ((float)adc / 4095.0) * 2.0 * 3.3 * (vref);
  return battery_voltage;
}

float readExternalBattery(int8_t pin) {

  int total = 0;
  int numAvg = 20;
  for (int i = 0; i < numAvg; i++) {
    total += analogRead(pin);
    delay(1);
  }
  int16_t adc = total / numAvg;
  float in_min = 2048;
  float in_max = 4096;
  float out_min = 8.25;
  float out_max = 16.5;
  return (float)(adc - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float readSBLT(int8_t pin) {
  int avg = 0;
  int8_t numMeasures = 10;
  for (int i = 0; i < numMeasures; i++) {
    avg = avg + analogRead(pin);
    delay(50);
  }
  Serial.print("Total: ");
  Serial.println(avg);
  int avgAdc = avg / numMeasures;
  Serial.print("SBLT: Average ADC: ");
  Serial.println(avgAdc);
  Serial.print("SBLT: Converted to MV raw steps (BEFORE ADJUSTMENT) 0.0008v: ");
  Serial.println(float(avgAdc * 0.0008), 3);
  avgAdc = avgAdc + 150;
  Serial.print("SBLT: Converted to MV raw steps (AFTER ADJUSTMENT) 0.0008v: ");
  Serial.println(float(avgAdc * 0.0008), 3);
  float y = 0.00527;
  float c = 2.5013;
  return (avgAdc * y) - c;
}

void measureSDI() {
  float values[2]; 
  // Initialize external variables not in shared.h
  ESP32_SDI12::Status res = sdi12.measure(sdi12Addr, values, sizeof(values), 2);
  if (res != ESP32_SDI12::SDI12_OK) {
    Serial.println("CRIT: SDI12 read error...");
    Serial.printf("Error: %d\n", res);
    waterLevel = -1;
    waterTemp = -1;
  } else {
    waterLevel = values[0];
    waterTemp = values[1];
    Serial.print("Water level: ");
    Serial.println(waterLevel);
    Serial.print("Water temp: ");
    Serial.println(waterTemp);
  }
}

void readVictron(unsigned long _timeout) {
  String line;
  float voltage = 0;
  float current = 0;
  unsigned long startTime = millis();
  bool currentFound = false;
  bool voltageFound = false;

  while (millis() - startTime < _timeout) {
    if (serialVictron.available()) {
      line = serialVictron.readStringUntil('\n');
      line.trim();

      int tabIndex = line.indexOf('\t');
      if (tabIndex != -1) {
        String key = line.substring(0, tabIndex);
        String value = line.substring(tabIndex + 1);

        if (key == "V") {
          voltage = value.toFloat() / 1000.0; // Convert mV to V
          Serial.print(F("Voltage: "));
          Serial.println(voltage);
          battVolt = voltage; // Update the global battVolt variable
          voltageFound = true;
        }
        if (key == "I") {
          current = value.toFloat(); // Already in mA
          Serial.print(F("Current: "));
          Serial.println(current);
          currentDraw = current;
          currentFound = true;
        }
        if (currentFound && voltageFound && firstVictronMeasurement) {
          averageCurrent = 0;
          averageCurrentCounter = 0;
          firstVictronMeasurement = false;
          currentFound = false;
          voltageFound = false;
        } else if (currentFound) {
          averageCurrent += currentDraw;
          averageCurrentCounter++;
        }

      }
    }
    vTaskDelay(1); // Small delay to prevent task from hogging CPU
  }

  if (voltage != 0 || current != 0) {
    Serial.printf("Victron Reading - Battery: %.3f V, Current: %.2f mA\n", voltage, current);
  } else {
    Serial.println(F("No valid Victron data read in this cycle"));
  }
}