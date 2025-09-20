#ifndef PRODUAL_MIO
#define PRODUAL_MIO

#include <memory>
#include "ModbusClient.h"
#include "mutexGuard.h"

// Register addresses (adjust according to your device documentation)
constexpr uint16_t REG_AO1 = 40001;        // AO1 value (Signed 16, 0–1000)
constexpr uint16_t REG_FAN_STATUS = 10001; // AI1 Digital Input (bit, 0=off, 1=on)

/**
 * @brief Class for controlling and monitoring a Produal MIO device over Modbus.
 *
 * This class uses RAII-based locking via MutexGuard to ensure that all
 * Modbus operations are thread-safe when used in a FreeRTOS environment.
 */
class ModbusMIO {
public:
    /**
     * @brief Constructs a ModbusMIO object.
     * @param modbus The shared pointer to the Modbus client.
     * @param slaveAddress The slave address of the Modbus device.
     * @param mutex Handle to a FreeRTOS mutex protecting Modbus access.
     */
    ModbusMIO(std::shared_ptr<ModbusClient> modbus,
              uint8_t slaveAddress,
              SemaphoreHandle_t mutex);

    /**
     * @brief Sets the ventilation fan speed.
     *
     * This function safely acquires the Modbus mutex using MutexGuard.
     * It writes the desired speed (0–100%) into the AO1 register
     * and then immediately checks whether the fan is running.
     *
     * @param percent Desired fan speed (0.0f – 100.0f).
     * @return True if the write succeeded and the fan reports as running,
     *         false otherwise.
     */
    [[nodiscard]] bool setFanSpeed(float percent) const;

    /**
     * @brief Checks whether the fan is currently running.
     *
     * This function safely acquires the Modbus mutex using MutexGuard.
     * It reads the digital input register and returns true if the fan
     * status bit is set.
     *
     * @return True if fan is running, false otherwise.
     */
    [[nodiscard]] bool isFanRunning() const;

private:
    std::shared_ptr<ModbusClient> modbus;
    uint8_t slaveAddress;
    SemaphoreHandle_t busMutex;
};

#endif
