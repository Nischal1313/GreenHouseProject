#include "sdp610.h"
#include <cstdio>
#include <cmath> // for NAN, powf
#include "pico/stdlib.h"

// SDP610 measurement command (see datasheet)
static constexpr uint8_t SDP610_MEASURE_CMD = 0xF1;

// Scale factors for different measurement modes (from datasheet chapter 2)
// Assuming standard mode for SDP610-125Pa
static constexpr float SCALE_FACTOR = 60.0f; // Pa/count for 125Pa range

SDP610::SDP610(i2c_inst_t *i2c_instance, uint sda_pin, uint scl_pin,
               SemaphoreHandle_t mutex, uint8_t i2c_address)
    : i2c(i2c_instance),
      sda_pin(sda_pin),
      scl_pin(scl_pin),
      busMutex(mutex),
      address(i2c_address),
      initialized(false) {}

bool SDP610::init() {
    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) return false;

    // Initialize I²C hardware
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    // Initialize I²C at 100kHz (standard speed)
    i2c_init(i2c, 100 * 1000);

    // Simple check if device responds
    uint8_t dummy;
    int result = i2c_read_blocking(i2c, address, &dummy, 1, false);

    initialized = (result >= 0);
    return initialized;
}

bool SDP610::startMeasurement() const {
    uint8_t cmd = SDP610_MEASURE_CMD;
    int result = i2c_write_blocking(i2c, address, &cmd, 1, false);
    return (result == 1);
}

bool SDP610::readData(uint8_t *data, size_t length) const {
    int result = i2c_read_blocking(i2c, address, data, length, false);
    return (result == static_cast<int>(length));
}

int16_t SDP610::readRawPressure() const {
    if (!initialized) return 0;

    MutexGuard lock(busMutex);
    if (!lock.owns_lock()) return 0;

    // Start measurement
    if (!startMeasurement()) {
        return 0;
    }

    // Wait for measurement to complete (datasheet specifies max 10ms)
    sleep_ms(10);

    // Read 3 bytes: 2 data bytes + 1 CRC byte (we'll ignore CRC for simplicity)
    uint8_t data[3];
    if (!readData(data, 3)) {
        return 0;
    }

    // Convert to 16-bit signed integer (big-endian)
    int16_t raw_value = (data[0] << 8) | data[1];
    return raw_value;
}

float SDP610::altitudeCorrection(float altitude_m) {
    // Simple altitude correction based on barometric formula
    // For more accurate correction, refer to datasheet chapter 5
    if (altitude_m == 0.0f) return 1.0f;

    // Approximate pressure correction factor
    // P = P0 * (1 - 0.0065 * h/288.15)^5.255
    return powf(1.0f - (0.0065f * altitude_m / 288.15f), 5.255f);
}

float SDP610::convertToPascals(int16_t raw_value, float altitude_m) {
    // Apply scale factor and altitude correction
    float correction = altitudeCorrection(altitude_m);
    return (raw_value / SCALE_FACTOR) * correction;
}

float SDP610::readPressurePa(float altitude_m) const {
    int16_t raw_value = readRawPressure();
    if (raw_value == 0) return NAN;

    return convertToPascals(raw_value, altitude_m);
}