#include "SDP610.h"
#include "pico/stdlib.h"
#include <cmath>

// Sensirion SDP610 commands
constexpr uint16_t SDP610_START_MEASUREMENT = 0x3603;  // Start continuous measurement
constexpr uint16_t SDP610_READ_MEASUREMENT  = 0xE000;  // Read measurement result
constexpr uint16_t SDP610_SOFT_RESET        = 0x0006;  // Soft reset

// Scale factors from datasheet
constexpr float SCALE_FACTOR_125PA = 60.0f;  // For 125Pa range

SDP610::SDP610(i2c_inst_t* i2cPort, uint8_t sensorAddr)
    : i2cPort(i2cPort), sensorAddr(sensorAddr) {}

bool SDP610::begin() {
    // Soft reset the sensor
    if (!writeCommand(SDP610_SOFT_RESET)) {
        return false;
    }

    sleep_ms(50); // Wait for reset to complete

    // Start continuous measurement
    return writeCommand(SDP610_START_MEASUREMENT);
}

int16_t SDP610::readRawPressure() {
    int16_t measurement = 0;
    if (readMeasurement(measurement)) {
        return measurement;
    }
    return 0; // Return 0 on error
}

float SDP610::readPressurePa(float altitude_m, float temperature_c) {
    int16_t raw_value = readRawPressure();
    float pressure_pa = applyScaleFactor(raw_value);
    return altitudeCompensation(pressure_pa, altitude_m, temperature_c);
}

float SDP610::applyScaleFactor(int16_t raw_value) {
    // Convert raw value to Pascals using scale factor
    return static_cast<float>(raw_value) / SCALE_FACTOR_125PA;
}

float SDP610::altitudeCompensation(float pressure_pa, float altitude_m, float temperature_c) {
    if (altitude_m == 0.0f) {
        return pressure_pa; // No compensation needed at sea level
    }

    // Simplified altitude compensation based on datasheet chapter 5
    // Linear approximation: pressure decreases by ~12 Pa per 100m altitude
    const float compensation_factor = 1.0f - (altitude_m * 0.00012f);
    return pressure_pa * compensation_factor;
}

bool SDP610::writeCommand(uint16_t command) {
    uint8_t buffer[2];
    buffer[0] = (command >> 8) & 0xFF; // MSB first
    buffer[1] = command & 0xFF;        // LSB

    int result = i2c_write_blocking(i2cPort, sensorAddr, buffer, 2, false);
    return result == 2;
}

bool SDP610::readMeasurement(int16_t& measurement) {
    uint8_t buffer[3]; // 2 bytes data + 1 byte CRC

    // Request measurement read
    if (!writeCommand(SDP610_READ_MEASUREMENT)) {
        return false;
    }

    // Read 3 bytes: 2 data bytes + CRC
    int result = i2c_read_blocking(i2cPort, sensorAddr, buffer, 3, false);
    if (result != 3) {
        return false;
    }

    // Verify CRC
    if (!checkCRC(buffer, 2, buffer[2])) {
        return false;
    }

    // Convert to signed 16-bit value (big-endian)
    measurement = (static_cast<int16_t>(buffer[0]) << 8) | buffer[1];
    return true;
}

uint8_t SDP610::calculateCRC8(const uint8_t* data, uint8_t length) {
    // Sensirion CRC-8 polynomial: x^8 + x^5 + x^4 + 1 (0x31)
    const uint8_t polynomial = 0x31;
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc = (crc << 1);
            }
        }
    }

    return crc;
}

bool SDP610::checkCRC(const uint8_t* data, uint8_t length, uint8_t checksum) {
    return calculateCRC8(data, length) == checksum;
}