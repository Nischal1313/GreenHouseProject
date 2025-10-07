#include "encoderHandler.h"

EncoderHandler::EncoderHandler(const uint pinA, const uint pinB)
    : pinA(pinA), pinB(pinB), lastStateA(0) {
    gpio_init(pinA);
    gpio_set_dir(pinA, GPIO_IN);
    gpio_pull_up(pinA);

    gpio_init(pinB);
    gpio_set_dir(pinB, GPIO_IN);
    gpio_pull_up(pinB);

    lastStateA = gpio_get(pinA);
}


int32_t EncoderHandler::readDelta() {
    int stateA = gpio_get(pinA);
    int stateB = gpio_get(pinB);

    int delta = 0;
    if (stateA != lastStateA) {
        // clockwise = increase
        if (stateA == stateB)
            delta = +1;
        else
            delta = -1;
    }
    lastStateA = stateA;
    return delta;
}
