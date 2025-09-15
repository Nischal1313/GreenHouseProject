#include "hmp60.h"

float HMP60::readTemperature() const {
    return readFloatRegisters(0x0000); // Register 0
}

float HMP60::readHumidity() const{
    return readFloatRegisters(0x0002); // Register 2
}

float HMP60::readDewPoint() const {
    return readFloatRegisters(0x0004); // Register 4
}