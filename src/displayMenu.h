#ifndef DISPLAY_MENU_H
#define DISPLAY_MENU_H

#include <memory>
#include <cstdio>

#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ssd1306os.h"
#include "rotary_encoder.h"
#include "sensor_handler.h"
#include "setCredentials.h"
#include "inputManager.h"
#include "debug.h"
#include "rotary_encoder.h"

enum class MenuState {
  MAIN,
  WIFI
};

struct DisplayParams {
  SensorHandler *sensorHandler;
  SetCredentials *credentials;
  InputManager *inputManager;
  ssd1306os *oLed;
  RotaryEncoder *encoder;
};

class DisplayManager {
public:
  DisplayManager(std::shared_ptr<Debug> debug,
                 std::shared_ptr<ssd1306os> oled,
                 const std::shared_ptr<RotaryEncoder> &encoderPtr);

  void setParams(DisplayParams *displayParams);
  static void taskEntry(void *pvParameters);

  [[noreturn]] void displayTask();

private:
  void drawMainMenu() const;
  void drawWifiMenu();
  void handleWifiMenuButtons();
  void changeMenu();
  void log(const char *fmt, ...) const;
  [[nodiscard]] MenuState getCurrentMenuState() const;

  std::shared_ptr<ssd1306os> oLed;
  std::shared_ptr<Debug> debug;
  std::shared_ptr<RotaryEncoder> encoder;

  DisplayParams *params;
  bool unsavedChanges;
  MenuState menuState;

  // Track previous button states to detect real changes
  bool prevSSIDSelected;
  bool prevCharsetChanged;
};

#endif  // DISPLAY_MENU_H