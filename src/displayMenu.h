// #ifndef DISPLAYMENU_H
// #define DISPLAYMENU_H
//
// #include <memory>
// #include "FreeRTOS.h"
// #include "task.h"
// #include "PicoI2C.h"
// #include "ssd1306os.h"
// #include "rotary_encoder.h"
// #include "sensor_handler.h"
// #include "setCredentials.h"
// #include "inputManager.h"
//
// // Holds references to system components used by DisplayManager
// struct DisplayParams {
//   SensorHandler *sensorHandler;
//   SetCredentials *credentials;
//   InputManager *inputManager;
//   ssd1306os *oLed;
//   RotaryEncoder *encoder;
// };
//
// class DisplayManager {
// public:
//   explicit DisplayManager(DisplayParams *params);
//
//   static void task_entry(void *param);  // Static entry for FreeRTOS
//   [[noreturn]] void display_task();     // Instance task
//
// private:
//   DisplayParams *params;
// };
//
// #endif

#ifndef DISPLAYMENU_H
#define DISPLAYMENU_H

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
  SetCredentials *credentials;
  InputManager *inputManager;
  ssd1306os *oLed;
  RotaryEncoder *encoder; // Add encoder back for WiFi menu
};

class DisplayManager {
public:
  explicit DisplayManager(std::shared_ptr<Debug> debug, std::shared_ptr<ssd1306os> oled,
                          const std::shared_ptr<RotaryEncoder> &encoderPtr);


  void setParams(DisplayParams *displayParams);

  [[noreturn]] void displayTask();

  static void taskEntry(void *pvParameters);

  // Inside the DisplayManager class declaration
  [[nodiscard]] MenuState getCurrentMenuState() const;

private:
  std::shared_ptr<ssd1306os> oLed;
  std::shared_ptr<Debug> debug;
  std::shared_ptr<RotaryEncoder> encoder; // For WiFi menu character selection

  DisplayParams *params;
  int lastWifiEncoderPos; // Track encoder position for WiFi menu
  bool unsavedChanges;
  MenuState menuState = MenuState::MAIN;

  void drawMainMenu() const;

  void drawWifiMenu();

  void changeMenu();

  void handleWifiMenuButtons();

  void log(const char *fmt, ...) const;

  static constexpr uint32_t AUTO_SAVE_INTERVAL_MS = 10'000; // 10 seconds
};

#endif
