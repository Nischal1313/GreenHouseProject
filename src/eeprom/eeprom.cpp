#include "eeprom.h"

#include <algorithm>
#include "pico/stdlib.h"

Eeprom::Eeprom(i2c_inst_t *pI2cPortP, uint8_t eepromAddrP, uint8_t addressWidthP) :
    i2cPortM{pI2cPortP},
    eepromAddrM{eepromAddrP},
    addressWidthM{addressWidthP}
{
}

void Eeprom::buildAddressBytes(int addrP, uint8_t *pOutP) const
{
    if (addressWidthM == 2)
    {
        pOutP[0] = static_cast<uint8_t>((addrP >> 8) & 0xFF);
        pOutP[1] = static_cast<uint8_t>(addrP & 0xFF);
    }
    else
    {
        pOutP[0] = static_cast<uint8_t>(addrP & 0xFF);
    }
}

int Eeprom::readByte(int addrP) const
{
    uint8_t addrBuf[2];
    buildAddressBytes(addrP, addrBuf);

    int result;

    uint8_t data = 0;
    int written = i2c_write_blocking(i2cPortM, eepromAddrM, addrBuf, addressWidthM, true);

    if (written != addressWidthM)
    {
        result = -1;
    }
    else
    {
        int read = i2c_read_blocking(i2cPortM, eepromAddrM, &data, 1, false);

        if (read != 1)
        {
            result = -1;
        }
        else
        {
            result = data;
        }
    }

    return result;
}

bool Eeprom::writeByte(int addrP, uint8_t dataP) const
{
    bool result;

    int existing = readByte(addrP);

    if (existing == static_cast<int>(dataP))
    {
        result = true;
    }
    else
    {
        uint8_t buf[3];
        buildAddressBytes(addrP, buf);
        buf[addressWidthM] = dataP;

        int written = i2c_write_blocking(
            i2cPortM,
            eepromAddrM,
            buf,
            addressWidthM + 1,
            false);

        if (written != (addressWidthM + 1))
        {
            result = false;
        }
        else
        {
            while (i2c_write_blocking(i2cPortM, eepromAddrM, buf, addressWidthM, true) < 0)
            {
                sleep_ms(1);
            }

            result = true;
        }
    }

    return result;
}

bool Eeprom::readBlock(int addrP, uint8_t *pBufferP, size_t lengthP) const
{
    bool result;

    uint8_t addrBuf[2];
    buildAddressBytes(addrP, addrBuf);

    if (i2c_write_blocking(i2cPortM, eepromAddrM, addrBuf, addressWidthM, true) != addressWidthM)
    {
        result = false;
    }
    else
    {
        result = i2c_read_blocking(i2cPortM, eepromAddrM, pBufferP, lengthP, false)
            == static_cast<int>(lengthP);
    }

    return result;
}

bool Eeprom::writeBlock(int addrP, uint8_t const *pBufferP, size_t lengthP)
{
    size_t const pageSizeM = 32;
    size_t bytesWritten = 0;
    bool result = true;

    while (bytesWritten < lengthP)
    {
        size_t chunk = std::min(
            pageSizeM - (static_cast<size_t>(addrP) % pageSizeM),
            lengthP - bytesWritten);

        uint8_t i2c_buffer[addressWidthM + chunk];
        buildAddressBytes(addrP, i2c_buffer);

        for (size_t i = 0; i < chunk; ++i)
        {
            i2c_buffer[addressWidthM + i] = pBufferP[bytesWritten + i];
        }

        int written = i2c_write_blocking(
            i2cPortM,
            eepromAddrM,
            i2c_buffer,
            addressWidthM + chunk,
            false);

        if (written != static_cast<int>(addressWidthM + chunk))
        {
            result = false;
            break;
        }

        sleep_ms(5);
        addrP += static_cast<int>(chunk);
        bytesWritten += chunk;
    }

    return result;
}
