#pragma once
#include <memory>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "mutexGuard.h"
#include "semphr.h"
#include "modbus/ModbusClient.h"

class ModbusSensorBase {
protected:
    std::shared_ptr<ModbusClient> modbus;
    SemaphoreHandle_t busMutex;
    uint8_t slaveAddress;

    // Helper: Prepares the Modbus target address safely
    [[nodiscard]] MutexGuard prepareModbus() const;

    // Core I/O functions
    [[nodiscard]] float readFloat(uint16_t address) const;
    [[nodiscard]] int16_t readInt(uint16_t address) const;
    [[nodiscard]] bool readBool(uint16_t address) const;

    [[nodiscard]] bool writeUInt(uint16_t address, uint16_t value) const;

    [[nodiscard]] bool writeFloat(uint16_t address, float value) const;

public:
    explicit ModbusSensorBase(std::shared_ptr<ModbusClient> modbus,
                              SemaphoreHandle_t mutex);

    virtual ~ModbusSensorBase() = default;
};
