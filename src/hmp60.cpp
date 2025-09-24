#include "hmp60.h"
#include "modbus/ModbusRegister.h"
#include <cstdio>
#include <cmath> // for NAN
#include <cstring> // for memcpy

HMP60::HMP60(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      busMutex(mutex),
      slaveAddress(241) {}

MutexGuard HMP60::prepareModbus() const {
    MutexGuard lock(busMutex);
    if (lock.owns_lock()) {
        modbus->set_destination_rtu_address(slaveAddress);
    }
    return lock;
}

float HMP60::readFloatFromHoldingRegisters(uint16_t address) const {
    auto lock = prepareModbus();
    if (!lock.owns_lock()) {
        return NAN;
    }

    uint16_t registers[2];
    int result = modbus->read_holding_registers(address, 2, registers);

    if (result != NMBS_ERROR_NONE) {
        return NAN;
    }
    // Register[1] is high word, Register[0] is low word
    const uint32_t combined = (static_cast<uint32_t>(registers[1]) << 16) | registers[0];
    float floatValue;
    memcpy(&floatValue, &combined, sizeof(floatValue));

    // Validate the value makes sense
    if (address == REG_HUMIDITY_FLOAT) {
        // Humidity should be between 0-100% RH
        if (floatValue >= 0.0f && floatValue <= 100.0f) {
            return floatValue;
        }
    }
    else if (address == REG_TEMPERATURE_FLOAT) {
        // Temperature should be between -40°C and +60°C for HMP60
        if (floatValue >= -40.0f && floatValue <= 60.0f) {
            return floatValue;
        }
    }

    return NAN;
}

float HMP60::readHumidity() const {
    return readFloatFromHoldingRegisters(REG_HUMIDITY_FLOAT);
}

float HMP60::readTemperature() const {
    return readFloatFromHoldingRegisters(REG_TEMPERATURE_FLOAT);
}