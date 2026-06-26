//
// Created by Keijo Länsikunnas on 14.2.2024.
//
#include <mutex>
#include "ModbusClient.h"
#include "pico/time.h"

ModbusClient::ModbusClient(std::shared_ptr<PicoOsUart> puartP) : uartM(puartP)
{
    platformConfM.transport = NMBS_TRANSPORT_RTU;
    platformConfM.read = uartTransportRead;
    platformConfM.write = uartTransportWrite;
    platformConfM.arg = static_cast<void*>(uartM.get());

    nmbs_error const err = nmbs_client_create(&nmbsM, &platformConfM);
    if (err != NMBS_ERROR_NONE)
    {
        // throw exception??
    }
    nmbs_set_destination_rtu_address(&nmbsM, 1);
    nmbs_set_read_timeout(&nmbsM, 1000);
    nmbs_set_byte_timeout(&nmbsM, 3);
}

int32_t ModbusClient::uartTransportRead(uint8_t* pBufP, uint16_t countP, int32_t byteTimeoutMsP, void* pArgP)
{
    auto* const puart = static_cast<PicoOsUart*>(pArgP);
    uint32_t timeout{byteTimeoutMsP < 0 ? portMAX_DELAY : static_cast<uint32_t>(byteTimeoutMsP)};
    uint const fifoLevel = puart->getFifoLevel();
    if (fifoLevel)
    {
        uint64_t fifoTimeout = ((fifoLevel * 10 + 32) * 1000000ULL) / puart->getBaud();
        fifoTimeout = fifoTimeout / 1000 + (fifoTimeout % 1000 ? 1 : 0);
        if (timeout < fifoTimeout)
        {
            timeout = fifoTimeout;
        }
    }
    int32_t readCount{0};
    int32_t cnt{0};
    do
    {
        cnt = puart->read(pBufP + readCount, countP - readCount, timeout);
        readCount += cnt;
    } while (readCount < countP && cnt > 0);

    return readCount;
}

int32_t ModbusClient::uartTransportWrite(uint8_t const* pBufP, uint16_t countP, int32_t byteTimeoutMsP, void* pArgP)
{
    return static_cast<PicoOsUart*>(pArgP)->write(pBufP, countP, byteTimeoutMsP);
}

void ModbusClient::setDestinationRtuAddress(uint8_t addressP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    nmbs_set_destination_rtu_address(&nmbsM, addressP);
}

nmbs_error ModbusClient::readCoils(uint16_t addressP, uint16_t quantityP, nmbs_bitfield coilsOutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_read_coils(&nmbsM, addressP, quantityP, coilsOutP);
}

nmbs_error ModbusClient::readDiscreteInputs(uint16_t addressP, uint16_t quantityP, nmbs_bitfield inputsOutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_read_discrete_inputs(&nmbsM, addressP, quantityP, inputsOutP);
}

nmbs_error ModbusClient::readHoldingRegisters(uint16_t addressP, uint16_t quantityP, uint16_t* pRegistersOutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_read_holding_registers(&nmbsM, addressP, quantityP, pRegistersOutP);
}

nmbs_error ModbusClient::readInputRegisters(uint16_t addressP, uint16_t quantityP, uint16_t* pRegistersOutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_read_input_registers(&nmbsM, addressP, quantityP, pRegistersOutP);
}

nmbs_error ModbusClient::writeSingleCoil(uint16_t addressP, bool valueP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_write_single_coil(&nmbsM, addressP, valueP);
}

nmbs_error ModbusClient::writeSingleRegister(uint16_t addressP, uint16_t valueP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_write_single_register(&nmbsM, addressP, valueP);
}

nmbs_error ModbusClient::writeMultipleCoils(uint16_t addressP, uint16_t quantityP, nmbs_bitfield const coilsP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_write_multiple_coils(&nmbsM, addressP, quantityP, coilsP);
}

nmbs_error ModbusClient::writeMultipleRegisters(uint16_t addressP, uint16_t quantityP, uint16_t const* pRegistersP)
{
    std::lock_guard<Fmutex> exclusive(accessM);
    return nmbs_write_multiple_registers(&nmbsM, addressP, quantityP, pRegistersP);
}
