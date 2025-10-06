#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "eeprom.h"

class RotaryEncoder {
public:
    RotaryEncoder();
    int currentRotationValue() const;
    static void encoderTask(void* pv);

private:
    static constexpr uint8_t PIN_A = 10;
    static constexpr uint8_t PIN_B = 11;
    static constexpr uint8_t EEPROM_ADDR = 0x00;
    static constexpr uint16_t EEPROM_CO2_ADDR = 0x10;

    int desiredCO2;
    int lastA, lastB;
    Eeprom eeprom;
    TaskHandle_t encoderTaskHandle;

    void readFromEEPROM();
    void writeToEEPROM() const;
};

#endif
