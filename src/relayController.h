#ifndef C02VALVE
#define C02VALVE

#include "pico/stdlib.h"

constexpr int VALVE_PIN = 27;
constexpr int MIN_WAIT_TIME_MS = 30000;   // 30s minimum wait between presses
constexpr int VALVE_OPEN_TIME_MS = 1800;  // 1.8s open time
constexpr int TASK_DELAY_TIME = 30;  // ms

class RELAYCONTROL {
public:
    explicit RELAYCONTROL(int buttonPin);
    void taskStep();

private:
    void openValve();
    void closeValve();

    int buttonPin;
    bool valveState;
    absolute_time_t lastOpenTime;
};

#endif
