#include <FreeRTOS.h>
#include "relayController.h"
#include "hardware/gpio.h"
#include "pico/time.h"

RELAYCONTROL::RELAYCONTROL(const int buttonPin, std::shared_ptr<Debug> debug)
    : buttonPin(buttonPin), valveState(false), m_debug(std::move(debug)) {
    lastOpenTime = get_absolute_time();

    // Init valve pin
    gpio_init(VALVE_PIN);
    gpio_set_dir(VALVE_PIN, GPIO_OUT);
    gpio_put(VALVE_PIN, false);

    // Init button pin
    gpio_init(buttonPin);
    gpio_set_dir(buttonPin, GPIO_IN);
    gpio_pull_up(buttonPin);

    m_debug->print("RelayControl initialized.\n");
}

void RELAYCONTROL::openValve() {
    gpio_put(VALVE_PIN, true);
    valveState = true;
    lastOpenTime = get_absolute_time();
    m_debug->print("VALVE: Opened (GPIO%d)\n", VALVE_PIN);
}

void RELAYCONTROL::closeValve() {
    gpio_put(VALVE_PIN, false);
    valveState = false;
    m_debug->print("VALVE: Closed (GPIO%d)\n", VALVE_PIN);
}

bool RELAYCONTROL::buttonPressed() const {
    return gpio_get(buttonPin) == 0;
}

bool RELAYCONTROL::canPressButton() const {
    if (valveState) return false;
    return (get_absolute_time() - lastOpenTime) >= make_timeout_time_ms(MIN_WAIT_TIME_MS);
}

void RELAYCONTROL::taskLoop() {
    while (true) {
        if (buttonPressed() && canPressButton()) {
            openValve();
            vTaskDelay(pdMS_TO_TICKS(VALVE_OPEN_TIME_MS));
            closeValve();
            vTaskDelay(pdMS_TO_TICKS(MIN_WAIT_TIME_MS));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
