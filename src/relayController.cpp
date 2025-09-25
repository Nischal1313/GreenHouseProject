#include "relayController.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>

RELAYCONTROL::RELAYCONTROL(int buttonPin)
    : buttonPin(buttonPin), valveState(false) {

    lastOpenTime = get_absolute_time();

    // Initialize valve pin
    gpio_init(VALVE_PIN);
    gpio_set_dir(VALVE_PIN, GPIO_OUT);
    gpio_put(VALVE_PIN, false);

    // Initialize button pin (active low)
    gpio_init(buttonPin);
    gpio_set_dir(buttonPin, GPIO_IN);
    gpio_pull_up(buttonPin);

}

void RELAYCONTROL::openValve() {
    gpio_put(VALVE_PIN, true);
    valveState = true;
    lastOpenTime = get_absolute_time();
    printf("Valve opened for 1.8 s \n");
    vTaskDelay(30);
}

void RELAYCONTROL::closeValve() {
    gpio_put(VALVE_PIN, false);
    valveState = false;
}

void RELAYCONTROL::taskStep() {
    if (!gpio_get(buttonPin)) {
        while (!gpio_get(buttonPin)) {
            vTaskDelay(TASK_DELAY_TIME);
        }

        int64_t elapsedMs = absolute_time_diff_us(lastOpenTime, get_absolute_time()) / 1000;
        printf("%lld ms elapsed. Wait until: %d ms)\n",
               elapsedMs, MIN_WAIT_TIME_MS);

        if (!valveState && elapsedMs >= MIN_WAIT_TIME_MS) {
            openValve();
            vTaskDelay(pdMS_TO_TICKS(VALVE_OPEN_TIME_MS));
            closeValve();
            lastOpenTime = get_absolute_time();
        }
    }
}