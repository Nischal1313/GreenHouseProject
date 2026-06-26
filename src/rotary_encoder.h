#pragma once

#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

class RotaryEncoder
{
public:
    explicit RotaryEncoder(uint pinAP = 10, uint pinBP = 11,
                           uint pinButtonP = 12,
                           uint32_t debounceTimeP = 5, // ms
                           uint32_t holdTimeP = 1000); // ms
    static void taskEntry(void *pvParametersP);
    void update();
    // All four bool functions 'consume' the event
    bool rotatedCW();
    bool rotatedCCW();

    bool buttonPressed();
    bool buttonHeld();

private:
    uint pinAM;
    uint pinBM;
    uint pinButtonM;
    int lastEncodedM;

    bool cwEventM;
    bool ccwEventM;
    bool lastButtonReadingM;
    bool buttonStateM;
    bool pressedEventM;
    bool heldEventM;

    absolute_time_t lastDebounceTimeM;
    absolute_time_t pressStartTimeM;
    uint32_t debounceTimeM;
    uint32_t holdTimeM;
};
