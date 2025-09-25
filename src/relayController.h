#ifndef C02VALVE
#define C02VALVE
#include <FreeRTOS.h>
#include "debug.h"

constexpr int VALVE_PIN   = 27;   // fixed valve pin
constexpr int BUTTON_PIN = 7;    // user button
constexpr int MIN_WAIT_TIME_MS = 30000;  // 30s wait
constexpr int VALVE_OPEN_TIME_MS = 1800; // 1.8s open time

class RELAYCONTROL {
public:
    RELAYCONTROL(int buttonPin, std::shared_ptr<Debug> debug);

    static void taskEntry(void* pvParameters) {
        const auto self = static_cast<RELAYCONTROL*>(pvParameters);
        self->taskLoop();
    }

private:
    void taskLoop();
    void openValve();
    void closeValve();
    [[nodiscard]] bool buttonPressed() const;
    [[nodiscard]] bool canPressButton() const;

    int buttonPin;
    bool valveState;
    absolute_time_t lastOpenTime;
    std::shared_ptr<Debug> m_debug;
};

#endif
