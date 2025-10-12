#include "relayController.h"

VALVE::VALVE()
    : valvePin(VALVE_PIN, GPIOMode::OUTPUT, GPIOPull::NONE, false),
      valveState(false)
{
    valvePin.write(false);
}

void VALVE::openValve() {
    valvePin.write(true);
    valveState = true;
}

void VALVE::closeValve() {
    valvePin.write(false);
    valveState = false;
}

bool VALVE::valveStatus() const {
    return valveState;
}
