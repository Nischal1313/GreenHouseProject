#pragma once
#include "gpio/gpio_pin.h"

constexpr uint VALVE_PIN = 27;

class VALVE {
public:
    VALVE();

    void openValve();
    void closeValve();
    [[nodiscard]] bool valveStatus() const;

private:
    GPIOPin valvePin;
    bool valveState;
};
