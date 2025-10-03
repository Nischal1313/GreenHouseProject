#include "produalMIO.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

#include "gmp252.h"

ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
                     const SemaphoreHandle_t mutex,
                     std::shared_ptr<ssd1306os> display)
    : modbus(std::move(modbus)),
      display(std::move(display)),
      slaveAddress(1),
      busMutex(mutex) {}

/**
 * Main CO₂ control loop: compare current vs desired and adjust
 */
void ModbusMIO::controlLoop(const GMP252& sensor, const int desiredValue) {
    int co2Lvl = static_cast<int>(sensor.readMeasuredCO2());

    // 1. Handle control
    handleValveLogic(co2Lvl, desiredValue);
}

void ModbusMIO::handleValveLogic(int co2Lvl, int desiredCo2Lvl) {
    if (co2Lvl < desiredCo2Lvl) {
        // inject CO₂
        valve.openValve();
        // keep fan off or low
        setFanSpeed(0.0f);
    }
    else if (co2Lvl > desiredCo2Lvl) {
        // flush out with fan
        setFanSpeed(100.0f);   // e.g. 80%
        valve.closeValve();
    }
    else {
        // maintain equilibrium
        setFanSpeed(00.0f);   // idle fan
        valve.closeValve();
    }
}
bool ModbusMIO::setFanSpeed(float percent) const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) return false;

    percent = std::clamp(percent, 0.0f, 100.0f);
    const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);

    modbus->set_destination_rtu_address(slaveAddress);
    nmbs_error err = modbus->write_single_register(REG_AO1, value);

    return (err == NMBS_ERROR_NONE);
}

bool ModbusMIO::isFanRunning() const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) return false;

    modbus->set_destination_rtu_address(slaveAddress);
    return true; // stub
}

float ModbusMIO::readFanSpeed() const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) return NAN;

    modbus->set_destination_rtu_address(slaveAddress);
    uint16_t value{};
    nmbs_error err = modbus->read_holding_registers(REG_AO1, 1, &value);
    if (err != NMBS_ERROR_NONE) return NAN;

    return (static_cast<float>(value) / 1000.0f) * 100.0f;
}
