#include "gmp252.h"
#include "modbus/ModbusRegister.h"
#include <cstdio>
#include <cmath> // for NAN
#include <cstring> // for memcpy

GMP252::GMP252(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      busMutex(mutex),
      slaveAddress(240) {} // GMP252 default Modbus address

MutexGuard GMP252::prepareModbus() const {
    MutexGuard lock(busMutex);
    if (lock.owns_lock()) {
        modbus->set_destination_rtu_address(slaveAddress);
    }
    return lock;
}

float GMP252::readFloatFromHoldingRegisters(uint16_t address) const {
    uint16_t registers[2];
    if (modbus->read_holding_registers(address, 2, registers) == NMBS_ERROR_NONE) {
        float result;
        memcpy(&result, registers, sizeof(result));
        return result;
    }
    return NAN;
}

int16_t GMP252::readInt16FromHoldingRegister(uint16_t address) const {
    uint16_t value;
    if (modbus->read_holding_registers(address, 1, &value) == NMBS_ERROR_NONE) {
        return static_cast<int16_t>(value);
    }
    return 0;
}

float GMP252::readMeasuredCO2() const {
    return readFloatFromHoldingRegisters(REG_MEASURED_CO2);
}

float GMP252::readCompensationT() const {
    return readFloatFromHoldingRegisters(REG_COMPENSATION_TEMP);
}

float GMP252::readMeasuredT() const {
    return readFloatFromHoldingRegisters(REG_MEASURED_TEMP);
}

int16_t GMP252::readCO2_16bit() const {
    return readInt16FromHoldingRegister(REG_CO2_16BIT);
}

int16_t GMP252::readCO2_16bit_scaled() const {
    return readInt16FromHoldingRegister(REG_CO2_16BIT_SCALED) * 10;
}