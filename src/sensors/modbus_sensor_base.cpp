#include "modbus_sensor_base.h"
#include <cstring>

ModbusSensorBase::ModbusSensorBase(std::shared_ptr<ModbusClient> pModbusP,
                                   SemaphoreHandle_t const mutexP)
    : modbusM(std::move(pModbusP)), busMutexM(mutexP)
{
}

MutexGuard ModbusSensorBase::prepareModbus() const
{
    MutexGuard lock(busMutexM);
    if (lock.owns_lock())
    {
        modbusM->setDestinationRtuAddress(getSlaveAddress());
    }
    return lock;
}

float ModbusSensorBase::readFloat(uint16_t const addressP) const
{
    auto const lock = prepareModbus();
    uint16_t regs[2]{};
    float result{NAN};

    if (lock.owns_lock())
    {
        int const res = modbusM->readHoldingRegisters(addressP, 2, regs);
        if (res == NMBS_ERROR_NONE)
        {
            uint32_t const raw = (static_cast<uint32_t>(regs[1]) << 16) | regs[0];
            std::memcpy(&result, &raw, sizeof(result));
        }
        else
        {
            printf("Modbus read error %d (addr=0x%04X, slave=%d)\n",
                   res, addressP, getSlaveAddress());
        }
    }

    return result;
}

int16_t ModbusSensorBase::readInt(uint16_t const addressP) const
{
    auto const lock = prepareModbus();
    int16_t result{static_cast<int16_t>(0xFFFF)};

    if (lock.owns_lock())
    {
        uint16_t value{};
        int const res = modbusM->readHoldingRegisters(addressP, 1, &value);
        if (res == NMBS_ERROR_NONE)
        {
            result = static_cast<int16_t>(value);
        }
        else
        {
            printf("Modbus read error %d (addr=0x%04X, slave=%d)\n",
                   res, addressP, getSlaveAddress());
        }
    }

    return result;
}

bool ModbusSensorBase::readBool(uint16_t const addressP) const
{
    auto const lock = prepareModbus();
    bool result{false};

    if (lock.owns_lock())
    {
        uint8_t value{};
        int const res = modbusM->readCoils(addressP, 1, &value);
        if (res == NMBS_ERROR_NONE)
        {
            result = (value != 0);
        }
        else
        {
            printf("Modbus coil read error %d (addr=0x%04X, slave=%d)\n",
                   res, addressP, getSlaveAddress());
        }
    }

    return result;
}

bool ModbusSensorBase::writeUInt(uint16_t const addressP, uint16_t const valueP) const
{
    auto const lock = prepareModbus();
    bool result{false};

    if (lock.owns_lock())
    {
        int const res = modbusM->writeSingleRegister(addressP, valueP);
        result = (res == NMBS_ERROR_NONE);
    }

    return result;
}

bool ModbusSensorBase::writeFloat(uint16_t const addressP, float const valueP) const
{
    auto const lock = prepareModbus();
    bool result{false};

    if (lock.owns_lock())
    {
        uint32_t raw;
        std::memcpy(&raw, &valueP, sizeof(valueP));
        uint16_t regs[2]{uint16_t(raw & 0xFFFF), uint16_t(raw >> 16)};
        int const res = modbusM->writeMultipleRegisters(addressP, 2, regs);
        result = (res == NMBS_ERROR_NONE);
    }

    return result;
}
