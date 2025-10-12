#pragma once
#include "modbus_sensor_base.h"

class GMP252 final : public ModbusSensorBase {
public:
    using ModbusSensorBase::ModbusSensorBase; // inherit constructor

    [[nodiscard]] float readMeasuredCO2() const;

private:
    static constexpr uint16_t REG_MEASURED_CO2 = 0x0000;
};
