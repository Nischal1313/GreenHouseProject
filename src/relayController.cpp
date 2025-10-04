#include "relayController.h"
#include "hardware/gpio.h"
#include "pico/time.h"


RELAYCONTROL::RELAYCONTROL()
    :valveState(false) {
    gpio_init(VALVE_PIN);
    gpio_set_dir(VALVE_PIN, GPIO_OUT);
    gpio_put(VALVE_PIN, false);
}

void RELAYCONTROL::openValve() {
    gpio_put(VALVE_PIN, true);
    valveState = true;
}

void RELAYCONTROL::closeValve() {
    gpio_put(VALVE_PIN, false);
    valveState = false;
}
bool RELAYCONTROL::valveStatus() const {
    return valveState;
}
