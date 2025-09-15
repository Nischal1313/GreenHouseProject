#ifndef GPIO_PIN_H
#define GPIO_PIN_H

#include "hardware/gpio.h"

/**
 * @class GPIOPin
 * @brief A simple wrapper class for Raspberry Pi Pico GPIO pins.
 *
 * This class abstracts GPIO pin initialization, configuration, and usage.
 * It supports:
 *   - Input or output mode
 *   - Optional pull resistors (up, down, or none)
 *   - Optional inversion of logic (active-low handling)
 *
 * Example usage:
 *   // Configure pin 16 as an input with pull-up and active-low
 *   GPIOPin button(16, true, GPIOPin::Pull::Up, true);
 *
 *   // Configure pin 25 (onboard LED) as output
 *   GPIOPin led(25, false);
 *
 *   // Read button state
 *   if (button()) { ... }
 *
 *   // Write to LED
 *   led(true);   // turn on
 *   led(false);  // turn off
 */
class GPIOPin {
private:
    int pin;       ///< The physical GPIO pin number
    bool invert;   ///< Whether the pin logic is inverted (active low)
    bool input;    ///< True if configured as input, false if output

public:
    /**
     * @enum Pull
     * @brief Defines the pull resistor configuration.
     */
    enum class Pull {
        None,   ///< No pull resistor
        Up,     ///< Enable pull-up
        Down    ///< Enable pull-down
    };

    /**
     * @brief Construct a GPIOPin and configure its mode.
     * @param pin GPIO pin number (0–29 on Pico)
     * @param input true = input, false = output
     * @param pull Pull resistor configuration (default: Pull::Up for inputs)
     * @param invert If true, logic is inverted (e.g. active-low button)
     *
     * Notes:
     *   - For inputs: sets direction and optional pull resistor.
     *   - For outputs: sets direction and initializes pin low.
     *   - Inversion is handled in software (read/write functions).
     */
    explicit GPIOPin(int pin, bool input = true, Pull pull = Pull::Up, bool invert = false);

    // Prevent accidental copying of pin objects (each pin is unique)
    GPIOPin(const GPIOPin &) = delete;

    /**
     * @brief Read the pin state (applies inversion if enabled).
     * @return true if logic HIGH (or active state if inverted)
     */
    [[nodiscard]] bool read() const;

    /**
     * @brief Write a value to the pin (only valid if configured as output).
     * @param value true = HIGH, false = LOW (inverted if enabled)
     */
    void write(bool value) const;

    /**
     * @brief Convenience operator for reading.
     * Allows using `if (pin) { ... }` instead of `if (pin.read())`.
     */
    bool operator()() const;

    /**
     * @brief Convenience operator for writing.
     * Allows using `pin(true)` instead of `pin.write(true)`.
     */
    void operator()(bool value) const;

    /**
     * @brief Conversion operator to return the pin number.
     * Allows implicit casting to int when needed.
     */
    explicit operator int() const;
};

#endif // GPIO_PIN_H
