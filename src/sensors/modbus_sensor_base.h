#pragma once
#include <memory>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "mutexGuard.h"
#include "semphr.h"
#include "modbus/ModbusClient.h"

class ModbusSensorBase
{
protected:
    std::shared_ptr<ModbusClient> modbusM;
    SemaphoreHandle_t busMutexM;

    // Pure virtual: each sensor must define its slave address
    [[nodiscard]] virtual uint8_t getSlaveAddress() const = 0;

    // Helper: Prepares the Modbus target address safely
    [[nodiscard]] MutexGuard prepareModbus() const;

    // Core I/O functions
    [[nodiscard]] float readFloat(uint16_t addressP) const;
    [[nodiscard]] int16_t readInt(uint16_t addressP) const;
    [[nodiscard]] bool readBool(uint16_t addressP) const;
    [[nodiscard]] bool writeUInt(uint16_t addressP, uint16_t valueP) const;
    [[nodiscard]] bool writeFloat(uint16_t addressP, float valueP) const;

public:
    explicit ModbusSensorBase(std::shared_ptr<ModbusClient> pModbusP,
                              SemaphoreHandle_t mutexP);

    virtual ~ModbusSensorBase() = default;
};
