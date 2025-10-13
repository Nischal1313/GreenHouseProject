#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H
#include "pico/stdlib.h"

class RotaryEncoder {
public:
  // Constructor
  explicit RotaryEncoder(uint pinA = 10, uint pinB = 11, uint pinButton = 12,
                         uint32_t debounceTime = 5, // ms
                         uint32_t holdTime = 1000); // ms

  // Call periodically (every 1–5 ms)
  void update();

  // Rotation detection
  bool rotatedCW();

  bool rotatedCCW();

  // Button detection
  bool buttonPressed();

  bool buttonHeld();

private:
  // Encoder pins
  uint pinA, pinB, pinButton;
  // Encoder state
  int lastEncoded; // previous A/B state
  bool cwEvent, ccwEvent;
  int lastA{}, lastB{};
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
