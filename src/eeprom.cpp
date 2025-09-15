#include "eeprom.h"
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

bool Eeprom::writeBlock(int addr, const uint8_t *buffer, size_t length) {
    // NOTE: EEPROMs usually have page size limits (e.g. 64 bytes per write).
    // This function should split writes into page-sized chunks.
    // For now, this is left as an exercise :)
    return false;
}
