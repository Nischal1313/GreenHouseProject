#include "modbus_fan.h"
#include <iostream>

ModbusMIO::ModbusMIO(const std::shared_ptr<ModbusClient> &modbus, const uint8_t slaveAddress)
    : ModbusSensor(modbus, slaveAddress) {}

bool ModbusMIO::setFanSpeed(float percent) const {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    // Scale percent to 0–1000 (0–10V in tenths of a volt if register expects mV)
    const uint16_t value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);

    modbus->set_destination_rtu_address(slaveAddress);
    nmbs_error err = modbus->write_single_register(REG_AO1, value);

    if (err != NMBS_ERROR_NONE) {
        std::cerr << "Failed to set fan speed, Modbus error: " << err << "\n";
        return false;
    }
    return true;
}

int16_t ModbusMIO::readFanPulseCounter() const {
    return readInt16Register(REG_AI1);
}

bool ModbusMIO::isFanRunning() const {
    // If two consecutive reads are zero, fan is not running
    const int16_t first = readFanPulseCounter();
    const int16_t second = readFanPulseCounter();
    return !(first == 0 && second == 0);
}
