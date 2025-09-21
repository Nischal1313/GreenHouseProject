#include "hmp60.h"
#include "modbus/ModbusRegister.h"
#include <cstdio>
#include <cmath> // for NAN
#include <cstring> // for memcpy

HMP60::HMP60(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      busMutex(mutex),
      slaveAddress(241) {} // HMP60 default Modbus address

MutexGuard HMP60::prepareModbus() const {
    MutexGuard lock(busMutex);
    if (lock.owns_lock()) {
        modbus->set_destination_rtu_address(slaveAddress);
    }
    return lock;
}

float HMP60::readFloatFromHoldingRegisters(uint16_t address) const {
    uint16_t registers[2];
    if (modbus->read_holding_registers(address, 2, registers) == NMBS_ERROR_NONE) {
        float result;
        memcpy(&result, registers, sizeof(result));
        return result;
    }
    return NAN;
}

int16_t HMP60::readInt16FromHoldingRegister(uint16_t address) const {
    uint16_t value;
    if (modbus->read_holding_registers(address, 1, &value) == NMBS_ERROR_NONE) {
        return static_cast<int16_t>(value);
    }
    return 0;
}

float HMP60::readHumidity() const {
    return readFloatFromHoldingRegisters(REG_HUMIDITY_FLOAT);
}

float HMP60::readTemperature() const {
    return readFloatFromHoldingRegisters(REG_TEMPERATURE_FLOAT);
}

int16_t HMP60::readHumidityScaled() const {
    return readInt16FromHoldingRegister(REG_HUMIDITY_INT);
}

int16_t HMP60::readTemperatureScaled() const {
    return readInt16FromHoldingRegister(REG_TEMPERATURE_INT);
}