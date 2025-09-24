// ===== gmp252.h file =====
#ifndef GMP252_H
#define GMP252_H

#include <memory>
#include "ModbusClient.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Vaisala GMP252 CO₂ sensor over Modbus.
 *
 * Device details:
 * - Slave address: 240
 * - Communication: Modbus RTU
 * - Registers provide CO₂ concentration and temperature values
 */
class GMP252 {
public:
    // === Modbus register map for GMP252 ===
    static constexpr uint16_t REG_MEASURED_CO2 = 0x0000; ///< Float, CO₂ concentration in ppm

    /**
     * @brief Constructs a GMP252 sensor object with parameters.
     * @param modbus Shared pointer to a Modbus client instance.
     */
    GMP252(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex);

    [[nodiscard]] float readMeasuredCO2() const;      ///< Register 0x0000 - CO₂ in ppm

private:
    std::shared_ptr<ModbusClient> modbus;
    uint8_t slaveAddress;
    bool initialized;  // Track if properly initialized

    /**
     * @brief Prepares for a Modbus operation by acquiring the lock and setting the slave address.
     * @return A MutexGuard object if successful, or an empty guard if failed.
     */
    /**
     * @brief Reads a float value from two consecutive holding registers.
     * @param address The starting register address.
     * @return The float value, or NAN if reading failed.
     */
    [[nodiscard]] float readFloatFromHoldingRegisters(uint16_t address) const;
};

#endif // GMP252_H
