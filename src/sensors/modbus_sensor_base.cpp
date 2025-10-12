#include "modbus_sensor_base.h"
#include <cstring>

ModbusSensorBase::ModbusSensorBase(std::shared_ptr<ModbusClient> modbus,
                                   const SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)), busMutex(mutex) {}

MutexGuard ModbusSensorBase::prepareModbus() const {
    MutexGuard lock(busMutex);
    if (lock.owns_lock()) {
        modbus->set_destination_rtu_address(slaveAddress);
    }
    return lock;
}

float ModbusSensorBase::readFloat(const uint16_t address) const {
    auto const lock = prepareModbus();
    if (!lock.owns_lock()) return NAN;

    uint16_t regs[2];
    const int res = modbus->read_holding_registers(address, 2, regs);
    if (res != NMBS_ERROR_NONE) {
        printf("Modbus read error %d (addr=0x%04X)\n", res, address);
        return NAN;
    }

    const uint32_t raw = (static_cast<uint32_t>(regs[1]) << 16) | regs[0];
    float value;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

int16_t ModbusSensorBase::readInt(const uint16_t address) const {
    auto const lock = prepareModbus();
    if (!lock.owns_lock()) return 0xFFFF;

    uint16_t value{};
    const int res = modbus->read_holding_registers(address, 1, &value);
    if (res != NMBS_ERROR_NONE) {
        printf("Modbus read error %d (addr=0x%04X)\n", res, address);
        return 0xFFFF;
    }
    return value;
}

bool ModbusSensorBase::readBool(const  uint16_t address) const {
    auto lock = prepareModbus();
    if (!lock.owns_lock()) return false;

    uint8_t value{};
    const int res = modbus->read_coils(address, 1, &value);
    if (res != NMBS_ERROR_NONE) {
        printf("Modbus coil read error %d (addr=0x%04X)\n", res, address);
        return false;
    }
    return value != 0;
}

bool ModbusSensorBase::writeUInt(const uint16_t address, const uint16_t value) const {
    auto lock = prepareModbus();
    if (!lock.owns_lock()) return false;

    const int res = modbus->write_single_register(address, value);
    return (res == NMBS_ERROR_NONE);
}

bool ModbusSensorBase::writeFloat(const uint16_t address, const float value) const {
    auto lock = prepareModbus();
    if (!lock.owns_lock()) return false;

    uint32_t raw;
    std::memcpy(&raw, &value, sizeof(value));
    uint16_t regs[2] = {uint16_t(raw & 0xFFFF), uint16_t(raw >> 16)};
    const int res = modbus->write_multiple_registers(address, 2, regs);
    return (res == NMBS_ERROR_NONE);
}