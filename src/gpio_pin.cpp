#include "gpio_pin.h"

/**
 * @brief GPIOPin constructor implementation.
 */
GPIOPin::GPIOPin(const int pin, const bool input, Pull pull, const bool invert)
    : pin(pin), invert(invert), input(input) {
    // Initialize the physical pin
    gpio_init(pin);

    if (input) {
        // Configure as input
        gpio_set_dir(pin, GPIO_IN);

        // Configure pull resistors
        if (pull == Pull::Up) {
            gpio_pull_up(pin);
        } else if (pull == Pull::Down) {
            gpio_pull_down(pin);
        }
        // Pull::None → do nothing
    } else {
        // Configure as output
        gpio_set_dir(pin, GPIO_OUT);

        // Initialize to LOW (safe default state)
        gpio_put(pin, false);
    }
}

/**
 * @brief Read the pin value, applying inversion if enabled.
 */
bool GPIOPin::read() const {
    bool raw = gpio_get(pin);
    return invert ? !raw : raw;
}

/**
 * @brief Write a value to the pin (only if output).
 */
void GPIOPin::write(const bool value) const {
    if (!input) {  // writing only allowed on outputs
        gpio_put(pin, invert ? !value : value);
    }
}

/**
 * @brief Operator() for reading: pin() acts like pin.read().
 */
bool GPIOPin::operator()() const {
    return read();
}

/**
 * @brief Operator() for writing: pin(value) acts like pin.write(value).
 */
void GPIOPin::operator()(const bool value) const {
    write(value);
}

/**
 * @brief Conversion operator to return the pin number.
 */
GPIOPin::operator int() const {
    return pin;
}
