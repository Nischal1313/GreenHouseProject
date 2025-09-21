#ifndef GMP252_H
#define GMP252_H

#include <memory>
#include "ModbusClient.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Vaisala GMP252 CO₂ sensor over Modbus.
 *
 * This class encapsulates Modbus communication for the GMP252
 * and uses RAII-based locking (`MutexGuard`) to ensure safe
 * concurrent access to the Modbus bus.
 *
 * Device details:
 * - Slave address: 240
 * - Communication: Modbus RTU
 * - Registers provide CO₂ concentration, compensation temperature,
 *   measured temperature, and 16-bit raw values.
 */
class GMP252 {
public:
    // === Modbus register map for GMP252 ===
    static constexpr uint16_t REG_MEASURED_CO2      = 0x0000; ///< Float, CO₂ concentration
    static constexpr uint16_t REG_COMPENSATION_TEMP = 0x0002; ///< Float, compensation temperature
    static constexpr uint16_t REG_MEASURED_TEMP     = 0x0004; ///< Float, measured temperature
    static constexpr uint16_t REG_CO2_16BIT         = 0x0100; ///< Raw 16-bit CO₂ concentration
    static constexpr uint16_t REG_CO2_16BIT_SCALED  = 0x0101; ///< Scaled 16-bit CO₂ (x10)

    /**
     * @brief Constructs a GMP252 sensor object.
     * @param modbus Shared pointer to a Modbus client instance.
     * @param mutex Handle to a FreeRTOS mutex for Modbus bus protection.
     */
    GMP252(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex);

    [[nodiscard]] float readMeasuredCO2() const;          ///< Register 0x0000
    [[nodiscard]] float readCompensationT() const;        ///< Register 0x0002
    [[nodiscard]] float readMeasuredT() const;            ///< Register 0x0004
    [[nodiscard]] int16_t readCO2_16bit() const;          ///< Register 0x0100
    [[nodiscard]] int16_t readCO2_16bit_scaled() const;   ///< Register 0x0101 (scaled x10)

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
     * @brief Reads a float value from two consecutive holding registers.
     * @param address The starting register address.
     * @return The float value, or NAN if reading failed.
     */
    [[nodiscard]] float readFloatFromHoldingRegisters(uint16_t address) const;

    /**
     * @brief Reads a 16-bit integer value from a holding register.
     * @param address The register address.
     * @return The 16-bit integer value, or 0 if reading failed.
     */
    [[nodiscard]] int16_t readInt16FromHoldingRegister(uint16_t address) const;
};

#endif // GMP252_H