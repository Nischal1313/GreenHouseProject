//
// Created by nischal on 10/10/25.
//

#include "rotary_encoder.h"

/*
 * Gray-code rotary encoder logic with button press/hold and debouncing.
 *
 * A/B signals form this Gray code pattern:
 *   CW:  00 → 01 → 11 → 10 → 00
 *   CCW: 00 → 10 → 11 → 01 → 00
 *
 * We use both last and current A/B states (2 bits each) to build a
 * 4-bit "transition code". Valid CW and CCW transitions are matched
 * against known patterns. This method filters bounce and invalid states.
 */

RotaryEncoder::RotaryEncoder(uint pinA, uint pinB, uint pinButton,
                             uint stepsPerRevolution,
                             uint32_t debounceTime, uint32_t holdTime)
    : pinA(pinA), pinB(pinB), pinButton(pinButton),
      position(0), fullRotations(0),
      stepsPerRevolution(stepsPerRevolution),
      lastEncoded(0), cwEvent(false), ccwEvent(false),
      lastButtonReading(false), buttonState(false),
      pressedEvent(false), heldEvent(false),
      debounceTime(debounceTime), holdTime(holdTime)
{
    // --- GPIO setup ---
    gpio_init(pinA); gpio_set_dir(pinA, GPIO_IN); gpio_pull_up(pinA);
    gpio_init(pinB); gpio_set_dir(pinB, GPIO_IN); gpio_pull_up(pinB);
    gpio_init(pinButton); gpio_set_dir(pinButton, GPIO_IN); gpio_pull_up(pinButton);

    int MSB = !gpio_get(pinA);
    int LSB = !gpio_get(pinB);
    lastEncoded = (MSB << 1) | LSB;

    lastDebounceTime = get_absolute_time();
    pressStartTime = get_absolute_time();
}

void RotaryEncoder::update() {
    // ---- ENCODER ROTATION ----
    const int MSB = !gpio_get(pinA);
    const int LSB = !gpio_get(pinB);
    const int encoded = (MSB << 1) | LSB;
    const int transition = (lastEncoded << 2) | encoded;

    // These 8 transitions are valid
    switch (transition) {
        // CW transitions
        case 0b1101: case 0b0100: case 0b0010: case 0b1011:
            position++;
            cwEvent = true;
            break;

        // CCW transitions
        case 0b1110: case 0b0111: case 0b0001: case 0b1000:
            position--;
            ccwEvent = true;
            break;

        default:
            // Ignore invalid transitions (bouncing or noise)
            break;
    }

    // Normalize to full rotations
    if (position >= stepsPerRevolution) {
        position = 0;
        fullRotations++;
    } else if (position <= -stepsPerRevolution) {
        position = 0;
        fullRotations--;
    }

    lastEncoded = encoded;

    // ---- BUTTON HANDLING ----
    const bool reading = !gpio_get(pinButton); // active low
    absolute_time_t now = get_absolute_time();

    // Debounce: only consider stable changes after debounceTime ms
    if (reading != lastButtonReading)
        lastDebounceTime = now;

    if (absolute_time_diff_us(lastDebounceTime, now) > debounceTime * 1000) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState) { // pressed
                pressedEvent = true;
                pressStartTime = now;
                heldEvent = false;
            } else {
                heldEvent = false;
            }
        }
    }

    // Detect hold
    if (buttonState && !heldEvent &&
        absolute_time_diff_us(pressStartTime, now) > holdTime * 1000) {
        heldEvent = true;
    }

    lastButtonReading = reading;
}

// --- EVENT ACCESSORS ---

bool RotaryEncoder::rotatedCW() {
    if (cwEvent) { cwEvent = false; return true; }
    return false;
}

bool RotaryEncoder::rotatedCCW() {
    if (ccwEvent) { ccwEvent = false; return true; }
    return false;
}

bool RotaryEncoder::buttonPressed() {
    if (pressedEvent) { pressedEvent = false; return true; }
    return false;
}

bool RotaryEncoder::buttonHeld() {
    if (heldEvent) { heldEvent = false; return true; }
    return false;
}

int RotaryEncoder::getPosition() const { return position; }
int RotaryEncoder::getFullRotations() const { return fullRotations; }
