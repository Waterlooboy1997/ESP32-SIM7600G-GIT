#include "OpusSensor.h"
#include <Arduino.h>

OpusSensor::OpusSensor(uint8_t modbusAddress, HardwareSerial& serial)
    : _modbusAddress(modbusAddress), _serialModbus(serial) {}

bool OpusSensor::initialize(int32_t modbus_baudrate, int8_t modbus_rx, int8_t modbus_tx) {
    _serialModbus.begin(modbus_baudrate, SERIAL_8N1, modbus_rx, modbus_tx);
    _modbus.begin(_modbusAddress, _serialModbus);
    return true;
}

bool OpusSensor::triggerMeasurement() {
    Serial.println("DBG: Attempting to trigger measurement...");
    mbresult = _modbus.writeSingleRegister(1, 0x0101);
    Serial.print("DBG: Modbus result for triggering measurement: ");
    Serial.println(mbresult);
    
    if (mbresult == 0) {
        Serial.println("DBG: Measurement triggered successfully.");
        return true;
    } else {
        Serial.println("DBG: Measurement trigger failed.");
        return false;
    }
}

bool OpusSensor::getMeasurement() {
    int8_t mbreadlen = 70;
    mbresult = _modbus.readHoldingRegisters(1000, mbreadlen);
    if (mbresult == _modbus.ku8MBSuccess) {
        for (int j = 0; j < mbreadlen - 1; j++) {
            data[j] = _modbus.getResponseBuffer(j);
        }
        int8_t measCounter = 0;
        union { int32_t x; float f; } u;
        for (int i = 0; i < sizeof(data) / 2; i += 2) {
            u.x = ((int32_t)data[i] << 16) | (uint16_t)data[i + 1];
            measurement[measCounter] = u.f;
            measCounter++;
        }
        _values[0] = measurement[0];  // OPUS_NNO3
        _values[1] = measurement[8];  // OPUS_TSSEQ
        _values[2] = measurement[30]; // OPUS_SQI
        _values[3] = measurement[17]; // OPUS_ABS360
        _values[4] = measurement[18]; // OPUS_ABS210
        _values[5] = measurement[21]; // OPUS_ABS254
        _values[6] = measurement[31]; // OPUS_UVT254
        _values[7] = measurement[16]; // OPUS_SAC254
        return true;
    } else {
        for (int i = 0; i < 8; i++) {
            _values[i] = 0;
        }
        return false;
    }
}

String OpusSensor::getMeasurementString() {
    return String(_values[0], 3) +
           "," + String(_values[1], 3) +
           "," + String(_values[2], 0) +
           "," + String(_values[3], 3) +
           "," + String(_values[4], 3) +
           "," + String(_values[5], 3) +
           "," + String(_values[6], 3) +
           "," + String(_values[7], 3);
}

String OpusSensor::getSensorName() {
    return "OPUS";
}

int8_t OpusSensor::getModbusResultCode() {
    return mbresult;
}

bool OpusSensor::getLastModbusCommunicationState() {
    return mbresult == _modbus.ku8MBSuccess ? false : true;
}

const float* OpusSensor::getValues() const {
    return _values;
}

int OpusSensor::getNumValues() const {
    return 8;
}

bool OpusSensor::readFirmwareVersion(char* firmwareVersion) {
    Serial.println("DBG: Attempting to read firmware version...");
    mbresult = _modbus.readHoldingRegisters(15, 5); // Reading 5 registers for 10 characters
    Serial.print("DBG: Modbus result for reading firmware version: ");
    Serial.println(mbresult);
    
    if (mbresult == _modbus.ku8MBSuccess) {
        Serial.println("DBG: Firmware version read successfully.");
        for (int i = 0; i < 10; i += 2) {
            firmwareVersion[i] = _modbus.getResponseBuffer(i / 2) >> 8;
            firmwareVersion[i + 1] = _modbus.getResponseBuffer(i / 2) & 0xFF;
        }
        firmwareVersion[10] = '\0';
        Serial.print("DBG: Firmware version: ");
        Serial.println(firmwareVersion);
        return true;
    } else {
        Serial.println("DBG: Failed to read firmware version.");
        return false;
    }
}


bool OpusSensor::readSystemDateTime(uint32_t& systemDateTime) {
    Serial.println("DBG: Attempting to read system date/time...");
    mbresult = _modbus.readHoldingRegisters(237, 2); // Reading 2 registers for a 32-bit value
    Serial.print("DBG: Modbus result for reading system date/time: ");
    Serial.println(mbresult);
    
    if (mbresult == _modbus.ku8MBSuccess) {
        systemDateTime = ((uint32_t)_modbus.getResponseBuffer(0) << 16) | (uint32_t)_modbus.getResponseBuffer(1);
        Serial.print("DBG: System date/time read successfully: ");
        Serial.println(systemDateTime, HEX);
        return true;
    } else {
        Serial.println("DBG: Failed to read system date/time.");
        return false;
    }
}