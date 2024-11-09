#ifndef NICOSENSOR_H
#define NICOSENSOR_H

#include "Sensor.h"
#include <ModbusMaster.h>

class NicoSensor : public Sensor {
public:
    static const int NUM_VALUES = 6;
    
    NicoSensor(uint8_t modbusAddress, HardwareSerial& serial);
    
    bool initialize(int32_t modbus_baudrate, int8_t modbus_rx, int8_t modbus_tx) override;
    bool triggerMeasurement() override;
    bool getMeasurement() override;
    String getMeasurementString() override;
    String getSensorName() override;
    int8_t getModbusResultCode() override;
    bool getLastModbusCommunicationState() override;
    bool readFirmwareVersion(char* firmwareVersion) override;
    bool readSystemDateTime(uint32_t& systemDateTime) override;
    
    // New method to access the float array of values
    const float* getValues() const override { return _values; }
    int getNumValues() const override { return NUM_VALUES; }

private:
    uint8_t _modbusAddress;
    ModbusMaster _modbus;
    HardwareSerial& _serialModbus;
    uint32_t _systemTimestamp;
    int16_t _rawData[80];
    float _measurement[40];
    float _values[NUM_VALUES];
    int8_t _mbresult;

    void processRawData();
};

#endif // NICOSENSOR_H