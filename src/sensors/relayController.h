#pragma once
#include "gpio/gpio_pin.h"

constexpr uint VALVE_PIN = 27;

class Valve
{
public:
    Valve();

    void openValve();
    void closeValve();
    [[nodiscard]] bool valveStatus() const;

private:
    GPIOPin valvePinM;
    bool valveStateM;
};
