#pragma once

#include "pico/stdlib.h"

enum class GPIOMode
{
    INPUT,
    OUTPUT
};

enum class GPIOPull
{
    NONE,
    PULLUP,
    PULLDOWN
};

class GPIOPin
{
public:
    explicit GPIOPin(
        uint pinP,
        GPIOMode modeP = GPIOMode::INPUT,
        GPIOPull pullP = GPIOPull::PULLUP,
        bool invertP = false,
        uint32_t debounceMsP = 50);

    [[nodiscard]] bool read() const;
    void write(bool valueP) const;
    [[nodiscard]] int getPin() const;

    void update();
    bool pressed();
    bool held();
    void setHoldTime(uint32_t msP);
    void setDebounceTime(uint32_t msP);

private:
    int pinNumberM;
    GPIOMode modeM;
    GPIOPull pullM;
    bool isInvertedM;

    bool lastReadingM;
    bool stableStateM;
    bool pressEventM;
    bool holdEventM;
    absolute_time_t lastChangeTimeM;
    absolute_time_t pressStartTimeM;
    uint32_t debounceMsM;
    uint32_t holdMsM;
};
