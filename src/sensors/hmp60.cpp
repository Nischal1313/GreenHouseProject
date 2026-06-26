#include "hmp60.h"
#include "modbus/ModbusRegister.h"

float HMP60::readHumidity() const
{
    float const relativeHumidity = readFloat(REG_HUMIDITY_FLOAT);
    float result{NAN};

    if (relativeHumidity >= 0.0f && relativeHumidity <= 100.0f)
    {
        result = relativeHumidity;
    }

    return result;
}

float HMP60::readTemperature() const
{
    float const temperature = readFloat(REG_TEMPERATURE_FLOAT);
    float result{NAN};

    if (temperature >= -40.0f && temperature <= 60.0f)
    {
        result = temperature;
    }

    return result;
}
