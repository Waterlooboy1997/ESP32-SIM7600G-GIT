#ifndef SDCARD_H
#define SDCARD_H

#include <FS.h>

class SDCard {
public:
    bool initialize();
    void appendFile(const char* path, const char* message);
    void writeFile(const char* path, const char* message);
    fs::FS& getSD();

private:
    bool sdInitialized = false;
};

#endif // SDCARD_H
