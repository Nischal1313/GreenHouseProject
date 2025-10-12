#include "produalMIO.h"
#include <algorithm>
#include "modbus/ModbusRegister.h"

bool ModbusMIO::setFanSpeed(float percent) const {
    percent = std::clamp(percent, 0.0f, 100.0f);
    const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);
    return writeUInt(REG_AO1, value);
}

float ModbusMIO::readFanSpeed() const {
    const auto raw = readInt(REG_AO1);
    return (raw <= 1000) ? (static_cast<float>(raw) / 10.0f) : NAN;
}