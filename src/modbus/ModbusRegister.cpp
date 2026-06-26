//
// Created by Keijo Länsikunnas on 14.2.2024.
//

#include "ModbusRegister.h"

ModbusRegister::ModbusRegister(std::shared_ptr<ModbusClient> pclientP, int serverAddressP, int registerAddressP,
                               bool holdingRegisterP) :
        clientM(pclientP), serverM(serverAddressP), regAddrM(registerAddressP), hrM(holdingRegisterP)
{
}

uint16_t ModbusRegister::read()
{
    uint16_t value{0};
    clientM->setDestinationRtuAddress(serverM);
    if (hrM)
    {
        clientM->readHoldingRegisters(regAddrM, 1, &value);
    }
    else
    {
        clientM->readInputRegisters(regAddrM, 1, &value);
    }
    return value;
}

void ModbusRegister::write(uint16_t valueP)
{
    if (hrM)
    {
        clientM->setDestinationRtuAddress(serverM);
        clientM->writeSingleRegister(regAddrM, valueP);
    }
}
