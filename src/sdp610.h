#ifndef SDP610_H
#define SDP610_H

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Sensirion SDP610 Differential Pressure Sensor over I²C.
 *
 * Notes:
 *  - This driver assumes the calling code initializes the I2C peripheral (i2c_init)
 *    and configures SDA/SCL pin functions/pull-ups once (done in main init).
 *  - The scale factor used in convertToPascals() is an example. Verify with the
 *    SDP610 datasheet and adjust FULL_SCALE_PA accordingly.
 */
class SDP610 {
public:
    SDP610(i2c_inst_t *i2c_instance, uint sda_pin, uint scl_pin,
           SemaphoreHandle_t mutex, uint8_t i2c_address = 0x40);

    // Initialize sensor (send any required commands and test a read)
    bool init();

    // Return raw signed 16-bit measurement (big-endian)
    [[nodiscard]] int16_t readRawPressure() const;

    // Convert raw value to pascals (applies altitude correction if altitude_m != 0)
    [[nodiscard]] float readPressurePa(float altitude_m = 0.0f) const;

    // Convert raw count to pascals (no I2C activity)
    float convertToPascals(int16_t raw_value, float altitude_m = 0.0f) const;

    // Altitude correction factor (returns multiplicative factor)
    static float altitudeCorrection(float altitude_m);

private:
    i2c_inst_t *i2c;
    uint sda_pin;
    uint scl_pin;
    SemaphoreHandle_t busMutex;
    uint8_t address;
    bool initialized;

    // Low-level operations
    bool softReset() const;
    bool startMeasurement() const;
    bool readMeasurement(int16_t &pressure) const;
    uint8_t calculateCRC(const uint8_t *data, size_t length) const;
};

#endif // SDP610_H
