#ifndef SDP610_H
#define SDP610_H

#include <cstdint>
#include "hardware/i2c.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Sensirion SDP610 Differential Pressure Sensor over I²C.
 *
 * This class encapsulates I²C communication for the SDP610
 * and uses RAII-based locking (`MutexGuard`) to ensure safe
 * concurrent access to the I²C bus.
 *
 * Device details:
 * - I²C address: 0x40
 * - Returns signed 16-bit differential pressure values
 * - Requires conversion to physical units (Pascals)
 * - May require altitude correction
 */
class SDP610 {
public:
    /**
     * @brief Constructs a SDP610 sensor object.
     * @param i2c_instance The I²C instance (i2c0 or i2c1).
     * @param sda_pin The SDA pin number.
     * @param scl_pin The SCL pin number.
     * @param mutex Handle to a FreeRTOS mutex for I²C bus protection.
     * @param i2c_address The I²C address of the sensor (default: 0x40).
     */
    SDP610(i2c_inst_t *i2c_instance, uint sda_pin, uint scl_pin,
           SemaphoreHandle_t mutex, uint8_t i2c_address = 0x40);

    /**
     * @brief Initializes the I²C communication with the sensor.
     * @return True if initialization successful, false otherwise.
     */
    bool init();

    /**
     * @brief Reads the raw differential pressure value from the sensor.
     * @return Raw 16-bit signed pressure value, or 0 if reading failed.
     */
    [[nodiscard]] int16_t readRawPressure() const;

    /**
     * @brief Reads and converts the differential pressure to Pascals.
     * @param altitude_m Altitude in meters for correction (default: 0 = sea level).
     * @return Differential pressure in Pascals, or NAN if reading failed.
     */
    [[nodiscard]] float readPressurePa(float altitude_m = 0.0f) const;

    /**
     * @brief Converts a raw pressure value to Pascals.
     * @param raw_value Raw 16-bit signed pressure value.
     * @param altitude_m Altitude in meters for correction.
     * @return Differential pressure in Pascals.
     */
    static float convertToPascals(int16_t raw_value, float altitude_m = 0.0f);

private:
    i2c_inst_t *i2c;
    uint sda_pin;
    uint scl_pin;
    SemaphoreHandle_t busMutex;
    uint8_t address;
    bool initialized;

    /**
     * @brief Sends a measurement command to the sensor.
     * @return True if command successful, false otherwise.
     */
    bool startMeasurement() const;

    /**
     * @brief Reads measurement data from the sensor.
     * @param data Buffer to store the read data.
     * @param length Number of bytes to read.
     * @return True if read successful, false otherwise.
     */
    bool readData(uint8_t *data, size_t length) const;

    /**
     * @brief Calculates altitude correction factor.
     * @param altitude_m Altitude in meters.
     * @return Correction factor.
     */
    static float altitudeCorrection(float altitude_m);
};

#endif // SDP610_H