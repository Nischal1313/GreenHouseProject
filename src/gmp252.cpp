#include "gmp252.h"
#include "modbus/ModbusRegister.h"
#include <cstdio>
#include <cmath> // for NAN
#include <cstring> // for memcpy

GMP252::GMP252(std::shared_ptr<ModbusClient> modbus, SemaphoreHandle_t mutex)
    : modbus(std::move(modbus)),
      slaveAddress(240) {
}

float GMP252::readFloatFromHoldingRegisters(uint16_t address) const {
    uint16_t registers[2];
    int result = modbus->read_holding_registers(address, 2, registers);

    if (result != NMBS_ERROR_NONE) {
        printf("GMP252: Modbus error %d for address 0x%04X\n", result, address);
        return NAN;
    }

    // Register[1] is high word, Register[0] is low word
    uint32_t combined = (static_cast<uint32_t>(registers[1]) << 16) | registers[0];
    float floatValue;
    memcpy(&floatValue, &combined, sizeof(floatValue));

    // Validate the value makes sense
    if (address == REG_MEASURED_CO2) {
        // CO2 should be between 0-5000 ppm (typical range)
        if (floatValue >= 0.0f && floatValue <= 10000.0f) {
            return floatValue;
        }
        return NAN;
    }
    return NAN;
}

float GMP252::readMeasuredCO2() const {
    return readFloatFromHoldingRegisters(REG_MEASURED_CO2);
}