#ifndef OPUSSENSOR_H
#define OPUSSENSOR_H

#include "Sensor.h"
#include <ModbusMaster.h>

class OpusSensor : public Sensor {
public:
    static const int NUM_VALUES = 8;
    OpusSensor(uint8_t modbusAddress, HardwareSerial& serial);
    bool initialize(int32_t modbus_baudrate, int8_t modbus_rx, int8_t modbus_tx) override;
    bool triggerMeasurement() override;
    bool getMeasurement() override;
    String getMeasurementString() override;
    String getSensorName() override;
    int8_t getModbusResultCode() override;
    bool getLastModbusCommunicationState() override;
    const float* getValues() const override;
    int getNumValues() const override;

    bool readFirmwareVersion(char* firmwareVersion);
    bool readSystemDateTime(uint32_t& systemDateTime);

private:
    uint8_t _modbusAddress;
    ModbusMaster _modbus;
    HardwareSerial& _serialModbus;
    int16_t data[80];
    float measurement[40];
    float _values[NUM_VALUES];  // To store the 8 OPUS measurements
    int8_t mbresult;
};

#endif // OPUSSENSOR_H