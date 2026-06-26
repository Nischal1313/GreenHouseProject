#pragma once
#include "modbus_sensor_base.h"

class ModbusMIO final : public ModbusSensorBase
{
public:
    using ModbusSensorBase::ModbusSensorBase;

    [[nodiscard]] bool setFanSpeed(float percentP) const;

    [[nodiscard]] float readFanSpeed() const;

protected:
    [[nodiscard]] uint8_t getSlaveAddress() const override
    {
        return SLAVE_ADDRESS;
    }

private:
    static constexpr uint16_t REG_AO1{0};
    static constexpr uint16_t REG_FAN_PULSE_COUNT{0};
    static constexpr uint8_t SLAVE_ADDRESS{1};
};
