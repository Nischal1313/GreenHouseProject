#include "eeprom.h"

#include <algorithm>

#include "pico/stdlib.h"

Eeprom::Eeprom(i2c_inst_t *i2cPort, uint8_t eepromAddr, uint8_t addressWidth)
    : i2cPort(i2cPort), eepromAddr(eepromAddr), addressWidth(addressWidth) {}

/**
 * @brief Helper: convert 16-bit/8-bit address into bytes
 */
void Eeprom::buildAddressBytes(int addr, uint8_t *out) const {
    if (addressWidth == 2) {
        out[0] = static_cast<uint8_t>((addr >> 8) & 0xFF);
        out[1] = static_cast<uint8_t>(addr & 0xFF);
    } else {
        out[0] = static_cast<uint8_t>(addr & 0xFF);
    }
}

int Eeprom::readByte(const int addr) const {
    uint8_t addrBuf[2];
    buildAddressBytes(addr, addrBuf);

    uint8_t data = 0;
    int written = i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true);
    if (written != addressWidth) {
        return -1; // Failed to send address
    }

    int read = i2c_read_blocking(i2cPort, eepromAddr, &data, 1, false);
    if (read != 1) {
        return -1; // Failed to read
    }

    return data;
}

bool Eeprom::writeByte(const int addr, const uint8_t data) const {
    int existing = readByte(addr);
    if (existing == data) {
        return true; // Skip write if value unchanged
    }

    uint8_t buf[3];
    buildAddressBytes(addr, buf);
    buf[addressWidth] = data;

    int written = i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth + 1, false);
    if (written != (addressWidth + 1)) {
        return false;
    }

    // Wait until EEPROM is ready (poll ACK instead of fixed delay)
    while (i2c_write_blocking(i2cPort, eepromAddr, buf, addressWidth, true) < 0) {
        sleep_ms(1);
    }

    return true;
}

bool Eeprom::readBlock(const int addr, uint8_t *buffer, const size_t length) const {
    uint8_t addrBuf[2];
    buildAddressBytes(addr, addrBuf);

    if (i2c_write_blocking(i2cPort, eepromAddr, addrBuf, addressWidth, true) != addressWidth) {
        return false;
    }

    return (i2c_read_blocking(i2cPort, eepromAddr, buffer, length, false) == (int)length);
}


// bool Eeprom::writeBlock(int addr, const uint8_t *buffer, size_t length) {
//     // We assume the caller (RotaryEncoder) is only writing 2 bytes (CO2 value),
//     // which fits within a single page write.
//
//     // Calculate the necessary buffer size: 2 address bytes + data length
//     const size_t i2c_buffer_size = addressWidth + length;
//     uint8_t i2c_buffer[4]; // Max size is 2 (addr) + 2 (data) = 4
//
//     if (i2c_buffer_size > sizeof(i2c_buffer)) {
//         // Data length exceeds the safe internal buffer size
//         return false;
//     }
//
//     // 1. Build the address bytes at the start of the temporary I2C buffer
//     buildAddressBytes(addr, i2c_buffer);
//
//     // 2. Copy the data block after the address
//     for (size_t i = 0; i < length; ++i) {
//         i2c_buffer[addressWidth + i] = buffer[i];
//     }
//
//     // 3. Write address and data in one go
//     int written = i2c_write_blocking(i2cPort, eepromAddr, i2c_buffer, i2c_buffer_size, false);
//
//     if (written != (int)i2c_buffer_size) {
//         return false; // Write failed
//     }
//
//     // Wait for the EEPROM write cycle to complete (typically 5ms for 24LCxx series)
//     // This is the simplest way to ensure the write operation finishes.
//     sleep_ms(5);
//
//     return true; // Write succeeded
// }

bool Eeprom::writeBlock(int addr, const uint8_t *buffer, size_t length) {
    // Limit writes to one page at a time (EEPROM page size, e.g., 32 bytes)
    const size_t pageSize = 32;
    size_t bytesWritten = 0;

    while (bytesWritten < length) {
        size_t chunk = std::min(pageSize - (addr % pageSize), length - bytesWritten);

        uint8_t i2c_buffer[addressWidth + chunk];
        buildAddressBytes(addr, i2c_buffer);

        for (size_t i = 0; i < chunk; i++) {
            i2c_buffer[addressWidth + i] = buffer[bytesWritten + i];
        }

        int written = i2c_write_blocking(i2cPort, eepromAddr, i2c_buffer, addressWidth + chunk, false);
        if (written != (int)(addressWidth + chunk)) return false;

        sleep_ms(5); // Wait for write cycle

        addr += chunk;
        bytesWritten += chunk;
    }

    return true;
}
