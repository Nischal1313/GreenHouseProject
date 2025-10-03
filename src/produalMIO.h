#ifndef PRODUAL_MIO
#define PRODUAL_MIO

#include <memory>

#include "gmp252.h"
#include "ModbusClient.h"
#include "mutexGuard.h"
#include "ssd1306os.h"
#include "relayController.h"


// Register addresses (zero-based offsets for Modbus functions)
static constexpr uint16_t REG_AO1 = 0;   // Holding register 40001 → AO1 fan speed
static constexpr uint16_t REG_FAN_PULSE_COUNT = 0; // Input register 30001 → fan pulse counter

class ModbusMIO {
public:
    ModbusMIO(std::shared_ptr<ModbusClient> modbus,
              SemaphoreHandle_t mutex,
              std::shared_ptr<ssd1306os> display);

    void controlLoop(const GMP252& sensor, int desiredValue);

    [[nodiscard]] bool setFanSpeed(float percent) const;
    [[nodiscard]] bool isFanRunning() const;
    [[nodiscard]] float readFanSpeed() const;

private:
    void handleValveLogic(int co2Lvl, int desiredCo2Lvl);
    void updateDisplay(int co2Lvl, int desiredCo2Lvl);

    std::shared_ptr<ModbusClient> modbus;
    std::shared_ptr<ssd1306os> display;
    RELAYCONTROL valve;   // internally managed
    uint8_t slaveAddress;
    SemaphoreHandle_t busMutex;
};

#endif
