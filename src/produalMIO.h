#ifndef PRODUAL_MIO
#define PRODUAL_MIO

#include <memory>
#include "gmp252.h"
#include "rotaryEncoder.h"
#include "ModbusClient.h"
#include "mutexGuard.h"
#include "relayController.h"

// Register addresses (zero-based offsets for Modbus functions)
static constexpr uint16_t REG_AO1 = 0;   // Holding register 40001 → AO1 fan speed
static constexpr uint16_t REG_FAN_PULSE_COUNT = 0; // Input register 30001 → fan pulse counter

class ModbusMIO {
public:
    ModbusMIO(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex);

    void controlLoop(const GMP252& sensor, const RotaryEncoder& rotaryEncoder);

    [[nodiscard]] bool setFanSpeed(float percent) const;
    [[nodiscard]] bool isFanRunning() const;
    [[nodiscard]] float readFanSpeed() const;

private:
    void handleValveLogic(int co2Lvl, int desiredCo2Lvl);

    std::shared_ptr<ModbusClient> modbus;
    RELAYCONTROL valve;
    uint8_t slaveAddress;
    SemaphoreHandle_t busMutex;
    RotaryEncoder encoder;
};

#endif
