// #include "displayMenu.h"
// #include <cstdio>
// #include "pico/stdlib.h"
//
// DisplayManager::DisplayManager(DisplayParams *params)
//     : params(params) {}
//
// void DisplayManager::task_entry(void *param) {
//   auto *self = static_cast<DisplayManager *>(param);
//   self->display_task();
// }
//
// [[noreturn]] void DisplayManager::display_task() {
//   char buf[128];
//   printf("wtd");
//
//   // Initialize local OLED (if not provided)
//   auto i2cbus = std::make_shared<PicoI2C>(1, 400000);
//   ssd1306os display(i2cbus);
//   display.fill(0);
//   // display.fill(1);
//   display.text("LOL", 20, 35);
// display.show();
//   printf("displayyyy\n");
//
//   while (true) {
//     printf("while tru\n");
//
//     // Pull live data from system
//     SensorValues values = params->sensorHandler->getReadings();
//     printf("values read\n");
//
//     snprintf(buf, sizeof(buf), "CO2: %.0f ppm", values.co2);
//     display.text(buf, 2, 5, 1);
//
//     snprintf(buf, sizeof(buf), "Set: %d ppm", values.targetCo2);
//     display.text(buf, 2, 15);
//
//     snprintf(buf, sizeof(buf), "Temp: %.1fC", values.temperature);
//     display.text(buf, 2, 25);
//
//     snprintf(buf, sizeof(buf), "Hum: %.1f%%", values.humidity);
//     display.text(buf, 2, 35);
//
//     snprintf(buf, sizeof(buf), "Fan: %.0f%%", values.fanSpeed);
//     display.text(buf, 2, 45);
//
//     snprintf(buf, sizeof(buf), "Valve: %s", values.valveOpen ? "OPEN" : "CLOSED");
//     display.text(buf, 2, 55);
//
//     display.show();
//
//     vTaskDelay(pdMS_TO_TICKS(200));
//   }
// }

#include "displayMenu.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>
#include "pico/stdio.h"

DisplayManager::DisplayManager(std::shared_ptr<Debug> debug, std::shared_ptr<ssd1306os> oled,
                               const std::shared_ptr<RotaryEncoder> &encoderPtr)
  : oLed(std::move(oled)),
    debug(std::move(debug)),
    encoder(encoderPtr),
    params(nullptr),
    lastWifiEncoderPos(0),
    unsavedChanges(false),
    menuState(MenuState::MAIN) {
  printf("[DisplayManager] Constructed with OLED=%p, encoder=%p\n",
         (void *) oLed.get(), (void *) encoder.get());
}

MenuState DisplayManager::getCurrentMenuState() const {
  return menuState;
}

void DisplayManager::setParams(DisplayParams *displayParams) {
  if (!displayParams) {
    printf("[DisplayManager] ERROR: displayParams is NULL!\n");
    return;
  }
  params = displayParams;
  printf("[DisplayManager] Parameters set successfully\n");
}

[[noreturn]] void DisplayManager::displayTask() {

  if (encoder->rotatedCW() || encoder->rotatedCCW() || encoder->buttonPressed()) {
    drawMainMenu();
  }


  int autoSaveCounter = 0;
  int updateCounter = 0;
  while (true) {
    // Handle MENU button press (switch between MAIN and WIFI menus)
    if (params->inputManager->getMenuPressEvent()) {
      printf("[DisplayManager] Menu button pressed! Current state: %s\n",
             menuState == MenuState::MAIN ? "MAIN" : "WIFI");
      changeMenu();
      updateCounter = 0; // Force immediate update
    }

    // Handle WiFi menu specific logic
    if (menuState == MenuState::WIFI) {
      handleWifiMenuButtons();
      if (encoder->rotatedCW()) {
        printf("[DisplayManager] Encoder CW in WiFi menu\n");
        params->credentials->rotateChar(1);
        unsavedChanges = true;
        updateCounter = 0;
      } else if (encoder->rotatedCCW()) {
        printf("[DisplayManager] Encoder CCW in WiFi menu\n");
        params->credentials->rotateChar(-1);
        unsavedChanges = true;
        updateCounter = 0;
      }

      // Check encoder button in WiFi menu
      if (encoder->buttonPressed()) {
        printf("[DisplayManager] Encoder button pressed in WiFi menu - adding char\n");
        params->credentials->confirmChar();
        unsavedChanges = true;
        updateCounter = 0;
      }
    }

    updateCounter++;
    if (updateCounter >= 1) {
      switch (menuState) {
        case MenuState::MAIN:
          drawMainMenu();
          break;
        case MenuState::WIFI:
          drawWifiMenu();
          break;
      }
      updateCounter = 0;
    }

    // // Auto-save WiFi credentials periodically
    // if (menuState == MenuState::WIFI) {
    //   autoSaveCounter += 50;
    //   if (unsavedChanges && autoSaveCounter >= AUTO_SAVE_INTERVAL_MS) {
    //     log("[DisplayManager] Auto-saving WiFi credentials\n");
    //     params->credentials->saveAllToEEPROM();
    //     unsavedChanges = false;
    //     autoSaveCounter = 0;
    //   }
    // } else {
    //   autoSaveCounter = 0;
    // }
    //

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void DisplayManager::handleWifiMenuButtons() {
  // NEXT FIELD button - move to next field (SSID/Password)
  if (params->inputManager->getNextFieldPressEvent()) {
    printf("[DisplayManager] Next Field button pressed in WiFi menu\n");
    params->credentials->nextField();
    unsavedChanges = true;
  }

  // CHARSET button - cycle through character sets (abc/ABC/123)
  if (params->inputManager->getCharsetPressEvent()) {
    printf("[DisplayManager] Charset button pressed in WiFi menu\n");
    params->credentials->nextCharset();
  }
}

void DisplayManager::drawMainMenu() const {
  oLed->fill(1);
  oLed->fill(0);

  char buf[128];
  // Get sensor readings
  const auto [temperature, humidity,
    co2, fanSpeed, valveOpen,
    targetCo2] = params->sensorHandler->getReadings();
  // Draw menu content
  snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
  oLed->text(buf, 2, 5);

  snprintf(buf, sizeof(buf), "Set: %d ppm", targetCo2);
  oLed->text(buf, 2, 15);

  snprintf(buf, sizeof(buf), "Temp: %.1fC", temperature);
  oLed->text(buf, 2, 25);

  snprintf(buf, sizeof(buf), "Hum: %.1f%%", humidity);
  oLed->text(buf, 2, 35);

  snprintf(buf, sizeof(buf), "Fan: %.0f%%", fanSpeed);
  oLed->text(buf, 2, 45);

  snprintf(buf, sizeof(buf), "Valve: %s", valveOpen ? "OPEN" : "CLOSED");
  oLed->text(buf, 2, 55);

  oLed->show();
}

void DisplayManager::drawWifiMenu() {
  oLed->fill(0);

  auto *cred = params->credentials;
  char buf[128];

  // Clear display
  oLed->fill(0);

  // Current field name (SSID or Password)
  snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
  oLed->text(buf, 2, 0);

  // Current buffer content
  snprintf(buf, sizeof(buf), "# %s", cred->getCurrentBuffer());
  oLed->text(buf, 2, 12);

  // Character set mode
  const char *charsetName =
      (cred->getCharsetMode() == CharsetMode::LOWERCASE)
        ? "abc"
        : (cred->getCharsetMode() == CharsetMode::UPPERCASE)
            ? "ABC"
            : (cred->getCharsetMode() == CharsetMode::NUMBERS)
                ? "123"
                : "undef";
  snprintf(buf, sizeof(buf), "Charset: %s", charsetName);
  oLed->text(buf, 2, 24);

  // Current character to add
  char curChar = cred->getCurrentChar();
  snprintf(buf, sizeof(buf), "Push: %c", curChar);
  oLed->text(buf, 2, 36);

  // Instructions
  oLed->text("Turn=Char", 2, 48);

  // Save status
  if (unsavedChanges)
    oLed->text("*UNSAVED*", 2, 56);
  else
    oLed->text("Saved", 2, 56);

  printf("[DisplayManager] WiFi menu drawn - Field: %s, Char: %c\n",
         cred->getCurrentFieldName(), curChar);
}

void DisplayManager::changeMenu() {
  if (menuState == MenuState::MAIN) {
    menuState = MenuState::WIFI;
    unsavedChanges = false;
    // } else {
    //   params->credentials->saveAllToEEPROM();
    //   vTaskDelay(pdMS_TO_TICKS(50));
    //   menuState = MenuState::MAIN;
    //   unsavedChanges = false;
    //   printf("[DisplayManager] ==> Switched to MAIN menu\n");
    //   log("[DisplayManager] Switched to MAIN menu\nSSID='%s' PW='%s'\n",
    //       params->credentials->getWifiSSID(),
    //       params->credentials->getWifiPassword());
  }
}

void DisplayManager::log(const char *fmt, ...) const {
  if (!debug) return;

  va_list args;
  va_start(args, fmt);

  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  debug->print("%s", buf);
}

void DisplayManager::taskEntry(void *pvParameters) {
  auto *self = static_cast<DisplayManager *>(pvParameters);
  self->displayTask();
}
