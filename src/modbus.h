// #ifndef MODBUS_SENSOR_H
// #define MODBUS_SENSOR_H
//
// #include <memory>
// #include "ModbusClient.h"
//
// class ModbusSensor {
// public:
//     ModbusSensor(std::shared_ptr<ModbusClient> modbus, uint8_t slaveAddress);
//
//     ModbusSensor();
//
//     ~ModbusSensor() = default;
//
// protected:
//     std::shared_ptr<ModbusClient> modbus;
//     uint8_t slaveAddress;
//
//     // Helper function to read registers and convert to float
//     [[nodiscard]] float readFloatRegisters(uint16_t address) const;
//     [[nodiscard]] int16_t readInt16Register(uint16_t address) const;
// };
//
// #endif // MODBUS_SENSOR_H