#include <algorithm>
#include <cstdio>
#include "produalMIO.h"

#include <cmath>

#include "pico/time.h"

ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
                     const SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      slaveAddress(1),   // Produal MIO Modbus address
      busMutex(mutex) {}

/**
 * @brief Checks if the ventilation fan is running by reading the pulse counter.
 *        The counter is self-clearing after each read.
 */
bool ModbusMIO::isFanRunning() const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) {
        printf("Failed to acquire Modbus mutex in isFanRunning()\n");
        return false;
    }
    modbus->set_destination_rtu_address(slaveAddress);
    return true;

}

/**
 * @brief Sets the fan speed (0–100%) and returns true if write succeeded.
 */
bool ModbusMIO::setFanSpeed(float percent) const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) {
        // printf("Failed to acquire Modbus mutex in setFanSpeed()\n");
        return false;
    }

    // Clamp to 0–100% and scale to 0–1000 (0–10 V)
    percent = std::clamp(percent, 0.0f, 100.0f);
    const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);

    modbus->set_destination_rtu_address(slaveAddress);
    nmbs_error err = modbus->write_single_register(REG_AO1, value);

    if (err == NMBS_ERROR_NONE) {
        return true;
    }
    return false;
}

float ModbusMIO::readFanSpeed() const {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) {
        printf("Failed to acquire Modbus mutex in readFanSpeed()\n");
        return NAN;
    }

    modbus->set_destination_rtu_address(slaveAddress);

    uint16_t value{};
    nmbs_error err = modbus->read_holding_registers(REG_AO1, 1, &value);
    if (err != NMBS_ERROR_NONE) {
        printf("Modbus read error %d in readFanSpeed()\n", err);
        return NAN;
    }

    // Convert 0–1000 (0–10V) → 0–100%
    return (static_cast<float>(value) / 1000.0f) * 100.0f;
}
