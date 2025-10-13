#include "gpio_pin.h"
#include <cstdio>

GPIOPin::GPIOPin(const uint pin, const GPIOMode mode,const GPIOPull pull,const bool invert,const uint32_t debounce_ms)
    : pin_number(pin), mode(mode), pull(pull), is_inverted(invert),
      lastReading(false), stableState(false),
      pressEvent(false), holdEvent(false),
      debounce_ms(debounce_ms), hold_ms(1000) // default hold = 1s
{
    gpio_init(pin);
    gpio_set_dir(pin, mode == GPIOMode::OUTPUT ? GPIO_OUT : GPIO_IN);

    // Configure pull resistors
    if (pull == GPIOPull::PULLUP)
        gpio_pull_up(pin);
    else if (pull == GPIOPull::PULLDOWN)
        gpio_pull_down(pin);

    // Optional inversion (hardware-level inversion)
    if (invert) {
        if (mode == GPIOMode::INPUT)
            gpio_set_inover(pin, GPIO_OVERRIDE_INVERT);
        else
            gpio_set_outover(pin, GPIO_OVERRIDE_INVERT);
    }

    lastChangeTime = get_absolute_time();
    pressStartTime = get_absolute_time();
    printf("[GPIO] init happened");
}

// --- READ/WRITE ---

bool GPIOPin::read() const {
    if (mode != GPIOMode::INPUT) {
        printf("[GPIO WARNING] Attempted to read from OUTPUT pin %d\n", pin_number);
        return false;
    }
    bool val = gpio_get(pin_number);
    return is_inverted ? !val : val;
}

void GPIOPin::write(const bool value) const {
    if (mode != GPIOMode::OUTPUT) {
        printf("[GPIO WARNING] Attempted to write to INPUT pin %d\n", pin_number);
        return;
    }
    gpio_put(pin_number, is_inverted ? !value : value);
}

int GPIOPin::getPin() const {
    return pin_number;
}

// --- BUTTON LOGIC ---

void GPIOPin::update() {
    if (mode != GPIOMode::INPUT) return; // ignore for output pins

    const bool reading = !gpio_get(pin_number); // active low button

    const absolute_time_t now = get_absolute_time();

    // Debounce filtering
    if (reading != lastReading)
        lastChangeTime = now;

    if (absolute_time_diff_us(lastChangeTime, now) > debounce_ms * 1000) {
        if (reading != stableState) {
            stableState = reading;

            if (stableState) {
                // Press started
                pressEvent = true;
                pressStartTime = now;
                holdEvent = false;
            } else {
                // Released
                holdEvent = false;
            }
        }
    }

    // Hold detection
    if (stableState && !holdEvent &&
        absolute_time_diff_us(pressStartTime, now) > hold_ms * 1000) {
        holdEvent = true;
    }

    lastReading = reading;
}

bool GPIOPin::pressed() {
    if (pressEvent) { pressEvent = false; return true; }
    return false;
}

bool GPIOPin::held() {
    if (holdEvent) { holdEvent = false; return true; }
    return false;
}

void GPIOPin::setHoldTime(const uint32_t ms) { hold_ms = ms; }
void GPIOPin::setDebounceTime(const uint32_t ms) { debounce_ms = ms; }
