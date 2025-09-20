// #include "modbus.h"
//
// #include <cstdint>
//
// #include "modbus.h"
// #include <cstring>
// #include "modbus/ModbusClient.h"
// #include "modbus/ModbusRegister.h"
// #include "modbus/nanomodbus.h"
//
// ModbusSensor::ModbusSensor(std::shared_ptr<ModbusClient> modbus, uint8_t slaveAddress)
//     : modbus(modbus), slaveAddress(slaveAddress) {}
//
// float ModbusSensor::readFloatRegisters(const uint16_t address) const {
//     uint16_t data[2];
//     modbus->set_destination_rtu_address(slaveAddress);
//     nmbs_error err = modbus->read_input_registers(address, 2, data);
//     if (err != NMBS_ERROR_NONE) {
//         return false; // Or handle error appropriately
//     }
//
//     // Convert two 16-bit registers to 32-bit float
//     const uint32_t combined = (static_cast<uint32_t>(data[0]) << 16) | data[1];
//     float value;
//     memcpy(&value, &combined, sizeof(float));
//     return value;
// }
//
// int16_t ModbusSensor::readInt16Register(const uint16_t address) const {
//     uint16_t data;
//     modbus->set_destination_rtu_address(slaveAddress);
//     nmbs_error err = modbus->read_input_registers(address, 1, &data);
//     if (err != NMBS_ERROR_NONE) {
//         return false;
//     }
//     return static_cast<int16_t>(data);
// }
