#ifndef C02VALVE
#define C02VALVE

#include "pico/stdio.h"


constexpr int VALVE_PIN = 27;
class RELAYCONTROL {
public:
    RELAYCONTROL();
    void openValve();
    void closeValve();

private:
    bool valveState;
    absolute_time_t lastOpenTime;
};

#endif
