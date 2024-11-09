#include "NicoSensor.h"
#include <Arduino.h>

NicoSensor::NicoSensor(uint8_t modbusAddress, HardwareSerial& serial)
    : _modbusAddress(modbusAddress), _serialModbus(serial) {
    memset(_values, 0, sizeof(_values));

}

bool NicoSensor::initialize(int32_t modbus_baudrate, int8_t modbus_rx, int8_t modbus_tx) {
    _serialModbus.begin(modbus_baudrate, SERIAL_8N1, modbus_rx, modbus_tx);
    _modbus.begin(_modbusAddress, _serialModbus);
    return true;
}

bool NicoSensor::triggerMeasurement() {
    _mbresult = _modbus.writeSingleRegister(1, 0x0101);
    return _mbresult == _modbus.ku8MBSuccess;
}

bool NicoSensor::getMeasurement() {
    if (!readSystemDateTime(_systemTimestamp)) {
        return false;
    }

    const int mbreadlen = 70;
    _mbresult = _modbus.readHoldingRegisters(1000, mbreadlen);
    if (_mbresult == _modbus.ku8MBSuccess) {
        for (int j = 0; j < mbreadlen - 1; j++) {
            _rawData[j] = _modbus.getResponseBuffer(j);
        }
        processRawData();
        return true;
    } else {
        memset(_values, 0, sizeof(_values));
        return false;
    }
}

void NicoSensor::processRawData() {
    union { int32_t x; float f; } u;
    int measCounter = 0;
    for (int i = 0; i < sizeof(_rawData) / 2; i += 2) {
        u.x = ((int32_t)_rawData[i] << 16) | (uint16_t)_rawData[i + 1];
        _measurement[measCounter++] = u.f;
    }
    
    _values[1] = _measurement[0];  // NNO3
    _values[2] = _measurement[2];  // SQI
    _values[3] = _measurement[3];  // REFA
    _values[4] = _measurement[4];  // REFB
    _values[5] = _measurement[5];  // REFC
    _values[6] = _measurement[6];  // REFD
}

String NicoSensor::getMeasurementString() {
    String result;
    for (int i = 0; i < NUM_VALUES; i++) {
        if (i > 0) result += ",";
        result += String(_values[i], 3);
    }
    return result;
}

String NicoSensor::getSensorName() {
    return "NICO";
}

int8_t NicoSensor::getModbusResultCode() {
    return _mbresult;
}

bool NicoSensor::getLastModbusCommunicationState() {
    return _mbresult != _modbus.ku8MBSuccess;
}

bool NicoSensor::readFirmwareVersion(char* firmwareVersion) {
    _mbresult = _modbus.readHoldingRegisters(15, 5); // Reading 5 registers for 10 characters
    if (_mbresult == _modbus.ku8MBSuccess) {
        for (int i = 0; i < 10; i += 2) {
            firmwareVersion[i] = _modbus.getResponseBuffer(i / 2) >> 8;
            firmwareVersion[i + 1] = _modbus.getResponseBuffer(i / 2) & 0xFF;
        }
        
        firmwareVersion[10] = '\0';
        return true;
    }
    return false;
}

bool NicoSensor::readSystemDateTime(uint32_t& systemDateTime) {
    _modbus.clearResponseBuffer();
    _modbus.clearTransmitBuffer();
    _mbresult = _modbus.readHoldingRegisters(237, 2); // Reading 2 registers for a 32-bit value
    if (_mbresult == _modbus.ku8MBSuccess) {
        systemDateTime = ((uint32_t)_modbus.getResponseBuffer(0) << 16) | (uint32_t)_modbus.getResponseBuffer(1);
        _systemTimestamp = systemDateTime;
        return true;
    }
    return false;
}