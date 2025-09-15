#ifndef EEPROM_H
#define EEPROM_H

#include "hardware/i2c.h"
#include <cstdint>

class Eeprom {
private:
    i2c_inst_t * const i2cPort;  ///< I2C port instance
    uint8_t eepromAddr;          ///< 7-bit EEPROM device address
    uint8_t addressWidth;        ///< Address size in bytes (1 or 2)

public:
    /**
     * @brief Construct an EEPROM interface.
     * @param i2cPort I2C instance (i2c0 or i2c1)
     * @param eepromAddr 7-bit device address (e.g. 0x50)
     * @param addressWidth Address width in bytes (default 2 for most EEPROMs)
     */
    Eeprom(i2c_inst_t *i2cPort, uint8_t eepromAddr, uint8_t addressWidth = 2);

    /**
     * @brief Read a single byte from EEPROM.
     * @param addr Memory address
     * @return Byte read, or -1 on error
     */
    [[nodiscard]] int readByte(int addr) const;

    /**
     * @brief Write a single byte to EEPROM (only if value differs).
     * @param addr Memory address
     * @param data Byte to write
     * @return true if write succeeded, false otherwise
     */
    bool writeByte(int addr, uint8_t data) const;

    /**
     * @brief Read multiple bytes into a buffer.
     */
    bool readBlock(int addr, uint8_t *buffer, size_t length) const;

    /**
     * @brief Write multiple bytes from a buffer.
     */
    static bool writeBlock(int addr, const uint8_t *buffer, size_t length);

private:
    void buildAddressBytes(int addr, uint8_t *out) const;
};

#endif // EEPROM_H
