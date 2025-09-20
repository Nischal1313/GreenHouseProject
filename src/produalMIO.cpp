#include <algorithm>
#include <cstdio>
#include "produalMIO.h"
#include "pico/time.h" // sleep_ms()

ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
                     uint8_t slaveAddress,
                     SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      slaveAddress(slaveAddress),
      busMutex(mutex) {}


/**
 * @brief Checks if the ventilation fan is running by reading the digital input.
 */
bool ModbusMIO::isFanRunning() const {
    MutexGuard lock(busMutex); // Acquire mutex safely
    if (!lock.owns_lock()) {
        printf("Failed to acquire Modbus mutex in ModbusMIO-> isFanRunning()\n");
        return false;
    }

    nmbs_bitfield status;
    if (modbus->read_discrete_inputs(REG_FAN_STATUS, 1, status) == NMBS_ERROR_NONE) {
        return (status[0] & 0x01) != 0;
    }
    return false;
}


/**
 * @brief Sets the fan speed and validates operation by checking fan status.
 */
bool ModbusMIO::setFanSpeed(float percent) const {
    MutexGuard lock(busMutex); // Acquire mutex safely
    if (!lock.owns_lock()) {
        printf("Failed to acquire Modbus mutex in setFanSpeed()\n");
        return false;
    }

    // Clamp percent to valid range and map to 0–1000 register scale
    percent = std::clamp(percent, 0.0f, 100.0f);
    const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);

    modbus->set_destination_rtu_address(slaveAddress);
    nmbs_error err = modbus->write_single_register(REG_AO1, value);

    if (err == NMBS_ERROR_NONE) {
        printf("Modbus write successful. Waiting to check fan status...\n");
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield CPU
        return isFanRunning();
    } else {
        printf("ERROR: %d. ", err);
        switch (err) {
            case NMBS_ERROR_CRC:
                printf("CRC error. Check wiring and device.\n");
                break;
            case NMBS_ERROR_TIMEOUT:
                printf("Request timeout. Device may not be connected or is offline.\n");
                break;
            case NMBS_ERROR_INVALID_RESPONSE:
                printf("Invalid response from device. Check slave address and function code.\n");
                break;
            case NMBS_ERROR_TRANSPORT:
                printf("Transport layer error.\n");
                break;
            default:
                printf("Unknown.\n");
                break;
        }
        return false;
    }
}
