#ifndef PRODUAL_MIO
#define PRODUAL_MIO

#include <memory>
#include "ModbusClient.h"
#include "mutexGuard.h"

// Register addresses (zero-based offsets for Modbus functions)
static constexpr uint16_t REG_AO1 = 0;   // Holding register 40001 → AO1 fan speed
static constexpr uint16_t REG_FAN_PULSE_COUNT = 0; // Input register 30001 → fan pulse counter

/**
 * @brief Class for controlling and monitoring a Produal MIO device over Modbus.
 */
class ModbusMIO {
public:
    ModbusMIO(std::shared_ptr<ModbusClient> modbus,
              SemaphoreHandle_t mutex);

    [[nodiscard]] bool setFanSpeed(float percent) const;
    [[nodiscard]] bool isFanRunning() const;

private:
    std::shared_ptr<ModbusClient> modbus;
    uint8_t slaveAddress;
    SemaphoreHandle_t busMutex;
};

#endif
