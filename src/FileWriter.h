#ifndef FILEWRITER_H
#define FILEWRITER_H

#include "FS.h"

class FileWriter {
public:
    void appendFile(fs::FS &fs, const char *path, const char *message);
    void writeFile(fs::FS &fs, const char *path, const char *message);
    bool checkFileExists(fs::FS &fs, const char *filename);
    int returnFileSize(fs::FS &fs, const char *filename);
    String readFile(fs::FS &fs, const char *filename);
    String readFileRowsFromEnd(fs::FS &fs, const char *filename, int32_t rows, bool includeHeader);

private:
    int rowsInStorage;
};

#endif // FILEWRITER_H
