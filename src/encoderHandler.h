#pragma once
#include <cstdint>
#include "hardware/gpio.h"

class EncoderHandler {
public:
    EncoderHandler(uint pinA, uint pinB);
    int32_t readDelta(); // returns +10, -10, or 0
private:
    uint pinA, pinB;
    int lastStateA;
};
