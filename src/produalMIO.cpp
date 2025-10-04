#include "produalMIO.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "gmp252.h"

ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
                     const SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      slaveAddress(1),
      busMutex(mutex) {}

void ModbusMIO::controlLoop(const GMP252& sensor, const RotaryEncoder& rotaryEncoder) {
    const int co2Lvl = static_cast<int>(sensor.readMeasuredCO2());
    const int desiredValue = rotaryEncoder.currentRotationValue();
    handleValveLogic(co2Lvl, desiredValue);
}

void ModbusMIO::handleValveLogic(const int co2Lvl,const int desiredCo2Lvl) {
    const int diff = desiredCo2Lvl - co2Lvl;

    // Within ±20 ppm → idle
    if (std::abs(diff) <= 20) {
        setFanSpeed(0.0f);
        valve.closeValve();
        return;
    }
    if (diff > 0) {
        const int valveTimeMs = (diff >= 500) ? 1000
                        : (diff >= 250) ? 500
                        : 150;

        valve.openValve();
        setFanSpeed(0.0f);
        vTaskDelay(pdMS_TO_TICKS(valveTimeMs));
        valve.closeValve();
    } else {
        setFanSpeed(100.0f);
        valve.closeValve();
        return;
    }
    // Idle fan after injection
    setFanSpeed(0.0f);
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
    return true; // placeholder
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
