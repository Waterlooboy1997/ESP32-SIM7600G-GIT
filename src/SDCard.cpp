#include "SDCard.h"
#include <SPI.h>
#include <SD.h>

bool SDCard::initialize() {
    if (!SD.begin()) {
        Serial.println("CRIT: SD mount fail...");
        return false;
    }
    Serial.println("DBG: SD success...");
    sdInitialized = true;
    return true;
}

void SDCard::appendFile(const char* path, const char* message) {
    if (!sdInitialized) {
        Serial.println("CRIT: SD not initialized...");
        return;
    }
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("CRIT: Failed to save to SD card...");
        return;
    }
    if (file.print(message)) {
        Serial.println("DBG: File appended...");
    } else {
        Serial.println("CRIT: Failed to save to SD card...");
    }
    file.close();
}

void SDCard::writeFile(const char* path, const char* message) {
    if (!sdInitialized) {
        Serial.println("CRIT: SD not initialized...");
        return;
    }
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("CRIT: Failed to open file for writing");
        return;
    }
    if (file.print(message)) {
        Serial.println("DBG: File written");
    } else {
        Serial.println("CRIT: Write failed");
    }
    file.close();
}

fs::FS& SDCard::getSD() {
    return SD;
}