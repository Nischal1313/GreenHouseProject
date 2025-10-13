#pragma once
#include "modbus_sensor_base.h"

class ModbusMIO final : public ModbusSensorBase {
public:
  using ModbusSensorBase::ModbusSensorBase;

  [[nodiscard]] bool setFanSpeed(float percent) const;

  [[nodiscard]] float readFanSpeed() const;

protected:
  [[nodiscard]] uint8_t getSlaveAddress() const override {
    return SLAVE_ADDRESS;
  }

private:
  // Register addresses (zero-based offsets for Modbus functions)
  static constexpr uint16_t REG_AO1 = 0; // Holding register 40001 → AO1 fan speed
  static constexpr uint16_t REG_FAN_PULSE_COUNT = 0; // Input register 30001 → fan pulse counter
  static constexpr uint8_t SLAVE_ADDRESS = 1;
};
