#ifndef HMP60_H
#define HMP60_H

#include <memory>
#include "ModbusClient.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Vaisala HMP60 Humidity and Temperature Probe over Modbus.
 *
 * Device details:
 * - Slave address: 241
 * - Communication: Modbus RTU
 * - Registers provide humidity and temperature values as 32-bit floats
 */
class HMP60 {
public:
    // === Modbus register map for HMP60 ===
    static constexpr uint16_t REG_HUMIDITY_FLOAT    = 0x0000; ///< Relative humidity, 32-bit float, %RH
    static constexpr uint16_t REG_TEMPERATURE_FLOAT = 0x0002; ///< Temperature, 32-bit float, °C

    /**
     * @brief Constructs a HMP60 sensor object.
     * @param modbus Shared pointer to a Modbus client instance.
     * @param mutex Handle to a FreeRTOS mutex for Modbus bus protection.
     */
    HMP60(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex);

    /**
     * @brief Reads relative humidity as a float value.
     * @return Relative humidity in %RH, or NAN if reading failed.
     */
    [[nodiscard]] float readHumidity() const;

    /**
     * @brief Reads temperature as a float value.
     * @return Temperature in °C, or NAN if reading failed.
     */
    [[nodiscard]] float readTemperature() const;

private:
    std::shared_ptr<ModbusClient> modbus;
    SemaphoreHandle_t busMutex;
    uint8_t slaveAddress;

    /**
     * @brief Prepares for a Modbus operation by acquiring the lock and setting the slave address.
     * @return A MutexGuard object if successful, or an empty guard if failed.
     */
    [[nodiscard]] MutexGuard prepareModbus() const;

    /**
     * @brief Reads a float value from two consecutive holding registers with proper byte order handling.
     * @param address The starting register address.
     * @return The float value, or NAN if reading failed.
     */
    [[nodiscard]] float readFloatFromHoldingRegisters(uint16_t address) const;
};

#endif // HMP60_H