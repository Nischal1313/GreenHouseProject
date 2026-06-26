#pragma once

#include "hardware/i2c.h"
#include <cstdint>

class Eeprom
{
public:
    Eeprom(i2c_inst_t *pI2cPortP, uint8_t eepromAddrP, uint8_t addressWidthP = 2);

    [[nodiscard]] int readByte(int addrP) const;
    bool writeByte(int addrP, uint8_t dataP) const;
    bool readBlock(int addrP, uint8_t *pBufferP, size_t lengthP) const;
    bool writeBlock(int addrP, uint8_t const *pBufferP, size_t lengthP);

private:
    i2c_inst_t * const i2cPortM;
    uint8_t eepromAddrM;
    uint8_t addressWidthM;

    void buildAddressBytes(int addrP, uint8_t *pOutP) const;
};
