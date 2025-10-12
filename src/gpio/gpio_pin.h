#ifndef GPIO_PIN_H
#define GPIO_PIN_H

#include "pico/stdlib.h"

// GPIO type: input or output
enum class GPIOMode {
    INPUT,
    OUTPUT
};

// Pull configuration
enum class GPIOPull {
    NONE,
    PULLUP,
    PULLDOWN
};

class GPIOPin {
public:
    // Default constructor (button input with pull-up, debounce = 50ms)
    explicit GPIOPin(uint pin,
            GPIOMode mode = GPIOMode::INPUT,
            GPIOPull pull = GPIOPull::PULLUP,
            bool invert = false,
            uint32_t debounce_ms = 50);

    // --- Core functions ---
    [[nodiscard]] bool read() const;
    void write(bool value) const;
    [[nodiscard]] int getPin() const;

    // --- Button logic ---
    void update();                // Call periodically (~ every 1–5 ms)
    bool pressed();               // true once per press
    bool held();                  // true once per hold
    void setHoldTime(uint32_t ms);
    void setDebounceTime(uint32_t ms);

private:
    int pin_number;
    GPIOMode mode;
    GPIOPull pull;
    bool is_inverted;

    // --- Button state ---
    bool lastReading;
    bool stableState;
    bool pressEvent;
    bool holdEvent;
    absolute_time_t lastChangeTime;
    absolute_time_t pressStartTime;
    uint32_t debounce_ms;
    uint32_t hold_ms;
};

#endif
