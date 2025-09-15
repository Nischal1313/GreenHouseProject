#include "gmp252.h"
#include "modbus.h"
#include "modbus/ModbusRegister.h"

float GMP252::readMeasuredCO2() const {
    return readFloatRegisters(0x0000); // Register 0
}

float GMP252::readCompensationT() const {
    return readFloatRegisters(0x0002); // Register 2
}

float GMP252::readMeasuredT() const {
    return readFloatRegisters(0x0004); // Register 4
}

int16_t GMP252::readCO2_16bit() const {
    return readInt16Register(0x0100); // Register 256
}

int16_t GMP252::readCO2_16bit_scaled() const {
    const int16_t value = readInt16Register(0x0101); // Register 257
    return value * 10; // Apply scaling factor
}
