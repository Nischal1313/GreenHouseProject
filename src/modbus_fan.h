#ifndef MODBUS_FAN
#define MODBUS_FAN

#include "modbus.h"

class ModbusMIO : public ModbusSensor {
public:
    ModbusMIO(const std::shared_ptr<ModbusClient> &modbus, uint8_t slaveAddress);

    // Set fan speed as percentage (0–100%)
    bool setFanSpeed(float percent) const;

    // Read current pulse counter (resets after read)
    int16_t readFanPulseCounter() const;

    // Check if fan is running
    bool isFanRunning() const;

private:
    // Modbus register addresses (example – check Produal MIO docs!)
    static constexpr uint16_t REG_AO1 = 0x0000;   // Analog output 1 (write)
    static constexpr uint16_t REG_AI1 = 0x0001;   // Digital input counter 1 (read)
};

#endif // MODBUS_MIO_H
