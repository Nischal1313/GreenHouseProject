#pragma once

#include <memory>
#include "PicoI2C.h"
#include "ssd1306os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rotary_encoder.h"
#include "setCredentials.h"
#include "sensor_handler.h"
#include "setpoint_manager.h"
#include "cloud_handler.h"
#include "debug.h"
#include "inputManager.h"
#include "pico/stdio.h"

enum class MenuState {
  MAIN,
  WIFI
};

struct DisplayParams {
  SensorHandler *sensorHandler;
  SetpointManager *setpointManager;
  RotaryEncoder *encoder;
  SetCredentials *credentials;
  InputManager *inputManager;
  ssd1306os *oLed;
};

class DisplayManager {
public:
  explicit DisplayManager(std::shared_ptr<Debug> debug, std::shared_ptr<ssd1306os> oled);

  void setParams(DisplayParams *displayParams);

  [[noreturn]] void displayTask();

  static void taskEntry(void *pvParameters);

private:
  std::shared_ptr<ssd1306os> oLed;

  DisplayParams *params;
  MenuState menuState;
  int lastEncoderValue;
  bool unsavedChanges;

  std::shared_ptr<Debug> debug;

  void drawMainMenu() const;

  void drawWifiMenu();

  void changeMenu();

  void handleWifiMenuButtons(); // New helper for WiFi menu buttons
  void log(const char *fmt, ...) const;

  static constexpr uint32_t AUTO_SAVE_INTERVAL_MS = 10'000; // 10 seconds
};
