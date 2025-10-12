#include "hmp60.h"
#include "modbus/ModbusRegister.h"

float HMP60::readHumidity() const {
    const float relativeHumidity = readFloat(REG_HUMIDITY_FLOAT);
    return (relativeHumidity >= 0.0f && relativeHumidity <= 100.0f) ? relativeHumidity : NAN;
}

float HMP60::readTemperature() const {
    const float temperature = readFloat(REG_TEMPERATURE_FLOAT);
    return (temperature >= -40.0f && temperature <= 60.0f) ? temperature : NAN;
}
