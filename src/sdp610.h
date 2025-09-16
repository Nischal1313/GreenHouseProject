#ifndef SDP610_H
#define SDP610_H

#include "hardware/i2c.h"
#include <cstdint>

class SDP610 {
private:
    i2c_inst_t* i2cPort;  ///< I2C port instance (i2c0 or i2c1)
    uint8_t sensorAddr;    ///< 7-bit I2C device address (0x40)

public:
    /**
     * @brief Construct an SDP610 differential pressure sensor interface.
     * @param i2cPort I2C instance (i2c0 or i2c1)
     * @param sensorAddr 7-bit device address (default 0x40)
     */
    SDP610(i2c_inst_t* i2cPort, uint8_t sensorAddr = 0x40);

    /**
     * @brief Initialize the sensor and start continuous measurement.
     * @return true if initialization succeeded, false otherwise
     */
    bool begin();

    /**
     * @brief Read raw differential pressure value.
     * @return Raw 16-bit signed value, or 0 on error
     */
    int16_t readRawPressure();

    /**
     * @brief Read and convert to Pascals with optional altitude compensation.
     * @param altitude_m Altitude in meters for compensation (default 0 = sea level)
     * @param temperature_c Temperature in Celsius for compensation (default 20°C)
     * @return Pressure in Pascals, or 0.0f on error
     */
    float readPressurePa(float altitude_m = 0.0f, float temperature_c = 20.0f);

    /**
     * @brief Apply scale factor to convert raw value to Pascals.
     * @param raw_value Raw 16-bit sensor reading
     * @return Pressure in Pascals
     */
    static float applyScaleFactor(int16_t raw_value);

    /**
     * @brief Apply altitude compensation to pressure reading.
     * @param pressure_pa Pressure in Pascals
     * @param altitude_m Altitude in meters
     * @param temperature_c Temperature in Celsius
     * @return Compensated pressure in Pascals
     */
    static float altitudeCompensation(float pressure_pa, float altitude_m, float temperature_c = 20.0f);

private:
    /**
     * @brief Write a 16-bit command to the sensor.
     * @param command 16-bit command value
     * @return true if write succeeded, false otherwise
     */
    bool writeCommand(uint16_t command);

    /**
     * @brief Read measurement data from sensor with CRC checking.
     * @param measurement Reference to store the read measurement
     * @return true if read succeeded, false otherwise
     */
    bool readMeasurement(int16_t& measurement);

    /**
     * @brief Calculate CRC-8 checksum for Sensirion sensors.
     * @param data Pointer to data buffer
     * @param length Number of bytes to calculate CRC for
     * @return CRC-8 checksum
     */
    uint8_t calculateCRC8(const uint8_t* data, uint8_t length);

    /**
     * @brief Verify CRC checksum for received data.
     * @param data Pointer to data buffer
     * @param length Number of data bytes
     * @param checksum Received checksum to verify
     * @return true if CRC matches, false otherwise
     */
    bool checkCRC(const uint8_t* data, uint8_t length, uint8_t checksum);
};

#endif // SDP610_H