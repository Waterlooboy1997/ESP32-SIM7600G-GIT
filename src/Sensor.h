#ifndef SENSOR_H
#define SENSOR_H

#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>

class Sensor {
public:
    static const int MAX_VALUES = 10; // Adjust this value as needed

    virtual bool initialize(int32_t modbus_baudrate, int8_t modbus_rx, int8_t modbus_tx) = 0;
    virtual bool triggerMeasurement() = 0;
    virtual bool getMeasurement() = 0;
    virtual String getMeasurementString() = 0;
    virtual String getSensorName() = 0;
    virtual int8_t getModbusResultCode() = 0;
    virtual bool getLastModbusCommunicationState() = 0;
    virtual bool readFirmwareVersion(char* firmwareVersion) = 0;
    virtual bool readSystemDateTime(uint32_t& systemDateTime) = 0;
    
    // New function to get the float array of values
    virtual const float* getValues() const = 0;
    
    // New function to get the number of values
    virtual int getNumValues() const = 0;

    virtual ~Sensor() {}
};

#endif // SENSOR_H