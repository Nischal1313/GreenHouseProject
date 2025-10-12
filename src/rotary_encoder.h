#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H
#include "pico/stdlib.h"

class RotaryEncoder {
public:
    // Constructor
    explicit RotaryEncoder(uint pinA = 11, uint pinB = 12, uint pinButton = 10,
                  uint stepsPerRevolution = 20, // typical mechanical encoder
                  uint32_t debounceTime = 5,    // ms
                  uint32_t holdTime = 1000);    // ms

    // Call periodically (every 1–5 ms)
    void update();

    // Rotation detection
    bool rotatedCW();
    bool rotatedCCW();

    // Button detection
    bool buttonPressed();
    bool buttonHeld();

    // Position and full rotations
    [[nodiscard]] int getPosition() const;
    [[nodiscard]] int getFullRotations() const;

private:
    // Encoder pins
    uint pinA, pinB, pinButton;

    // Encoder state
    int position;             // position in detents
    int fullRotations;        // number of completed turns
    int stepsPerRevolution;   // steps for one full turn
    int lastEncoded;          // previous A/B state
    bool cwEvent, ccwEvent;

    // Button state
    bool lastButtonReading;
    bool buttonState;
    bool pressedEvent;
    bool heldEvent;
    absolute_time_t lastDebounceTime;
    absolute_time_t pressStartTime;
    uint32_t debounceTime;
    uint32_t holdTime;
};

#endif
