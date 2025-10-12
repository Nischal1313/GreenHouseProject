#include "gmp252.h"
#include "modbus/ModbusRegister.h"

float GMP252::readMeasuredCO2() const {
    const float co2 = readFloat(REG_MEASURED_CO2);
    if (co2 < 0.0f || co2 > 10000.0f)
        return NAN;
    return co2;
}
