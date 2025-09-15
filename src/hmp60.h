#ifndef HMP60_H
#define HMP60_H

#include "modbus.h"

class HMP60 : public ModbusSensor {
public:
    explicit HMP60(const std::shared_ptr<ModbusClient> &modbus)
        : ModbusSensor(modbus, 241) {}  // Slave address 241

    float readTemperature() const;      // Register 0 (0x0000)
    float readHumidity() const;

    float readDewPoint() const;

};

#endif // HMP60_H