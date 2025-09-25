#include <algorithm>
#include <cstdio>
#include "produalMIO.h"
#include "pico/time.h"

ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus)
    : modbus(std::move(modbus)),
      slaveAddress(1)   // Produal MIO Modbus address
       {}

bool ModbusMIO::isFanRunning() const {
    modbus->set_destination_rtu_address(slaveAddress);
    return true;
}

bool ModbusMIO::setFanSpeed(float percent) const {
    // Clamp to 0–100% and scale to 0–1000 (0–10 V)
    percent = std::clamp(percent, 0.0f, 100.0f);
    const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);

    modbus->set_destination_rtu_address(slaveAddress);
    nmbs_error err = modbus->write_single_register(REG_AO1, value);

    if (err == NMBS_ERROR_NONE) {
        return true;
    }
    return false;
}
