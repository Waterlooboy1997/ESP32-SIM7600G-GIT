#include "FileWriter.h"

void FileWriter::appendFile(fs::FS &fs, const char *path, const char *message) {
    File file = fs.open(path, FILE_APPEND);
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

void FileWriter::writeFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("DBG: Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
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

bool FileWriter::checkFileExists(fs::FS &fs, const char *filename) {
    bool doesFileExist = false;
    File file = fs.open(filename);
    if (!file) {
        Serial.print("DBG: ");
        Serial.print(filename);
        Serial.println(" does NOT exist...");
    } else {
        Serial.print("DBG: ");
        Serial.print(filename);
        Serial.println(" file EXISTS...");
        doesFileExist = true;
    }
    file.close();
    return doesFileExist;
}

int FileWriter::returnFileSize(fs::FS &fs, const char *filename) {
    int size = 0;
    File file = fs.open(filename, FILE_READ);
    if (!file) {
        Serial.println("CRIT: Failed to open file for reading...");
    } else {
        size = file.size();
    }
    file.close();
    return size;
}

String FileWriter::readFile(fs::FS &fs, const char *filename) {
    String ret;
    File file = fs.open(filename, FILE_READ);
    if (!file) {
        Serial.println("CRIT: Failed to open file for reading...");
    } else {
        while (file.available()) {
            ret += file.readString();
        }
        file.close();
        return ret;
    }
    return "err";
}

String FileWriter::readFileRowsFromEnd(fs::FS &fs, const char *filename, int32_t rows, bool includeHeader) {
    String ret;
    String headers;
    File file = fs.open(filename, FILE_READ);
    if (!file) {
        Serial.println("CRIT: Failed to open file for reading...");
    } else {
        int32_t count = 0;
        while (file.available()) {
            String dummyStr = file.readStringUntil('\n');
            if (count == 0) {
                headers = dummyStr;
            }
            count++;
        }

        Serial.print("DBG: Rows in .csv: ");
        Serial.println(count);
        rowsInStorage = count;

        // Check if requested row count exceeds rows in storage
        if (rowsInStorage <= rows || rows == 0) {
            // read entire file...
            file.seek(0); // Seek back to position 0 in the file
            if (!includeHeader) file.readStringUntil('\n'); // if we don't want headers, skip first line.
            while (file.available()) {
                ret += file.readString();
            }
        } else {
            // To grab the last 'x' amount of rows we subtract the total count from the rows parameter
            int32_t startCollectionRow = (count - rows) - 1;

            if (startCollectionRow < 0) {
                startCollectionRow = 0;
            }

            if (includeHeader) {
                ret += headers + "\n"; // prepend headers to file
            }

            if (startCollectionRow < 1) {
                startCollectionRow = 1;
            }
            Serial.print("DBG: Looking for rows between: ");
            Serial.print(startCollectionRow);
            Serial.print(" to ");
            Serial.println(count);
            file.seek(0); // Seek back to position 0 in the file
            int32_t rowCounter = 0;
            while (file.available()) {
                String dummyStr = file.readStringUntil('\n');
                if (rowCounter >= startCollectionRow) {
                    ret += dummyStr + "\n";
                }
                rowCounter++;
            }
        }
        file.close();
    }

    // Escape newlines for JSON
    ret.replace("\n", "\\n");

    // Return JSON string
    String jsonResponse = ret;
    return jsonResponse;
}
