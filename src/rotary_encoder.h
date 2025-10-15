#pragma once

#include "pico/stdlib.h"

class RotaryEncoder {
public:
  explicit RotaryEncoder(uint pinA = 10, uint pinB = 11,
                         uint pinButton = 12,
                         uint32_t debounceTime = 5, // ms
                         uint32_t holdTime = 1000); // ms

  void update();
  //All four bool functions 'consume' the event
  bool rotatedCW();
  bool rotatedCCW();

  bool buttonPressed();
  bool buttonHeld();

private:
  uint pinA, pinB, pinButton;
  int lastEncoded;

  bool cwEvent, ccwEvent;
  bool lastButtonReading;
  bool buttonState;
  bool pressedEvent;
  bool heldEvent;

  absolute_time_t lastDebounceTime;
  absolute_time_t pressStartTime;
  uint32_t debounceTime;
  uint32_t holdTime;
};
