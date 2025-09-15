#ifndef GMP252_H
#define GMP252_H

#include <cstdint>

#include "modbus.h"

class GMP252 : public ModbusSensor {
public:
    GMP252(const std::shared_ptr<ModbusClient> &modbus)
        : ModbusSensor(modbus, 240) {}  // Slave address 240

    float readMeasuredCO2() const;          // Register 0 (0x0000)
    float readCompensationT() const;        // Register 2 (0x0002)
    float readMeasuredT() const;            // Register 4 (0x0004)
    int16_t readCO2_16bit() const;          // Register 256 (0x0100)
    int16_t readCO2_16bit_scaled() const;   // Register 257 (0x0101)
};

#endif // GMP252_H