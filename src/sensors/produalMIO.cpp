#include "produalMIO.h"
#include <algorithm>
#include "modbus/ModbusRegister.h"

bool ModbusMIO::setFanSpeed(float percentP) const
{
    float const clamped = std::clamp(percentP, 0.0f, 100.0f);
    uint16_t const value = static_cast<uint16_t>((clamped / 100.0f) * 1000.0f);
    return writeUInt(REG_AO1, value);
}

float ModbusMIO::readFanSpeed() const
{
    int16_t const raw = readInt(REG_AO1);
    float result{NAN};

    if (raw <= 1000)
    {
        result = static_cast<float>(raw) / 10.0f;
    }

    return result;
}
