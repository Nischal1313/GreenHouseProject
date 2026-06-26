//
// Created by Keijo Länsikunnas on 14.2.2024.
//

#pragma once

#include <memory>
#include "ModbusClient.h"

class ModbusRegister
{
public:
    ModbusRegister(std::shared_ptr<ModbusClient> pclientP, int serverAddressP, int registerAddressP, bool holdingRegisterP = true);
    uint16_t read();
    void write(uint16_t valueP);

private:
    std::shared_ptr<ModbusClient> clientM;
    int serverM;
    int regAddrM;
    bool hrM;
};
