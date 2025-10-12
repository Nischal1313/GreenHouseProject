#pragma once
#include "modbus_sensor_base.h"

class HMP60 final : public ModbusSensorBase {
public:
    using ModbusSensorBase::ModbusSensorBase;

    [[nodiscard]] float readHumidity() const;

    [[nodiscard]] float readTemperature() const;

private:
    static constexpr uint16_t REG_HUMIDITY_FLOAT = 0x0000; ///< Relative humidity, 32-bit float
    static constexpr uint16_t REG_TEMPERATURE_FLOAT = 0x0002; ///< Temperature, 32-bit float, °C
};
