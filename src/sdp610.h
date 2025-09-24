#ifndef SDP610_H
#define SDP610_H

#include "hardware/i2c.h"
#include "semphr.h"
#include "mutexGuard.h"

/**
 * @brief Driver for the Sensirion SDP610 Differential Pressure Sensor over I²C.
 *
 * Notes:
 *  - This driver assumes the calling code initializes the I2C peripheral (i2c_init)
 *    and configures SDA/SCL pin functions/pull-ups once (done in main init).
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

    // Altitude correction factor (returns multiplicative factor)
    static float altitudeCorrection(float altitude_m);

private:
    i2c_inst_t *i2c;
    uint sda_pin;
    uint scl_pin;
    SemaphoreHandle_t busMutex;
    uint8_t address;
    bool initialized;
};

#endif // SDP610_H
