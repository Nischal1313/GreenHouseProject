#pragma once
#include "modbus_sensor_base.h"

class GMP252 final : public ModbusSensorBase
{
public:
    using ModbusSensorBase::ModbusSensorBase;

    [[nodiscard]] float readMeasuredCO2() const;

protected:
    [[nodiscard]] uint8_t getSlaveAddress() const override
    {
        return SLAVE_ADDRESS;
    }

private:
    static constexpr uint16_t REG_MEASURED_CO2{0x0000};
    static constexpr uint8_t SLAVE_ADDRESS{240};
};
