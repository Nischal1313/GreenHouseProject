#include "relayController.h"

Valve::Valve()
    : valvePinM(VALVE_PIN, GPIOMode::OUTPUT, GPIOPull::NONE, false),
      valveStateM(false)
{
    valvePinM.write(false);
}

void Valve::openValve()
{
    valvePinM.write(true);
    valveStateM = true;
}

void Valve::closeValve()
{
    valvePinM.write(false);
    valveStateM = false;
}

bool Valve::valveStatus() const
{
    return valveStateM;
}
