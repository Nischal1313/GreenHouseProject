#include "gmp252.h"
#include "modbus/ModbusRegister.h"

float GMP252::readMeasuredCO2() const
{
    float const co2 = readFloat(REG_MEASURED_CO2);
    float result{NAN};

    if (co2 >= 0.0f && co2 <= 10000.0f)
    {
        result = co2;
    }

    return result;
}
