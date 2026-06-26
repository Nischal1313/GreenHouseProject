//
// Created by Keijo Länsikunnas on 14.2.2024.
//

#pragma once

#include <memory>
#include "nanomodbus.h"
#include "uart/PicoOsUart.h"

// Wrapper class does not implement full nanomodbus API
// addresses are wire addresses (numbering starts from zero)
class ModbusClient
{
public:
    explicit ModbusClient(std::shared_ptr<PicoOsUart> puartP);
    void setDestinationRtuAddress(uint8_t addressP);
    nmbs_error readCoils(uint16_t addressP, uint16_t quantityP, nmbs_bitfield coilsOutP);
    nmbs_error readDiscreteInputs(uint16_t addressP, uint16_t quantityP, nmbs_bitfield inputsOutP);
    nmbs_error readHoldingRegisters(uint16_t addressP, uint16_t quantityP, uint16_t* pRegistersOutP);
    nmbs_error readInputRegisters(uint16_t addressP, uint16_t quantityP, uint16_t* pRegistersOutP);
    nmbs_error writeSingleCoil(uint16_t addressP, bool valueP);
    nmbs_error writeSingleRegister(uint16_t addressP, uint16_t valueP);
    nmbs_error writeMultipleCoils(uint16_t addressP, uint16_t quantityP, nmbs_bitfield const coilsP);
    nmbs_error writeMultipleRegisters(uint16_t addressP, uint16_t quantityP, uint16_t const* pRegistersP);

private:
    static int32_t uartTransportRead(uint8_t* pBufP, uint16_t countP, int32_t byteTimeoutMsP, void* pArgP);
    static int32_t uartTransportWrite(uint8_t const* pBufP, uint16_t countP, int32_t byteTimeoutMsP, void* pArgP);

    std::shared_ptr<PicoOsUart> uartM;
    nmbs_platform_conf platformConfM;
    nmbs_t nmbsM;
    Fmutex accessM;
};
