extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "setCredentials.h"
#include "displayMenu.h"
#include <cstdarg>
#include <cstring>
#include "pico/stdio.h"


DisplayManager::DisplayManager(std::shared_ptr<Debug> debug,
                               std::shared_ptr<ssd1306os> oled,
                               const std::shared_ptr<RotaryEncoder> &
                               encoderPtr)
  : oLed(std::move(oled)),
    debug(std::move(debug)),
    encoder(encoderPtr),
    params(nullptr),
    unsavedChanges(false),
    menuState(MenuState::MAIN),
    prevSSIDSelected(true),
    prevCharsetChanged(false) {
}

void DisplayManager::setParams(DisplayParams *displayParams) {
  if (!displayParams) {
    printf("[DisplayManager] ERROR: displayParams is NULL!\n");
    return;
  }
  params = displayParams;
}

MenuState DisplayManager::getCurrentMenuState() const {
  return menuState;
}

[[noreturn]] void DisplayManager::displayTask() {
  while (true) {
    if (!params || !params->inputManager) {
      printf("[DisplayManager] Waiting for valid params...\n");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    bool inMainMenu = params->inputManager->isMainMenu();

    if (inMainMenu) {
      drawMainMenu();
    } else {
      handleWifiMenuButtons();
      drawWifiMenu();
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void DisplayManager::drawMainMenu() const {
  oLed->fill(0);

  char buf[128];
  const auto [temperature, humidity, co2, fanSpeed, valveOpen,
        targetCo2] =
      params->sensorHandler->getReadings();

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

  snprintf(buf, sizeof(buf), "Valve: %s",
           valveOpen ? "OPEN" : "CLOSED");
  oLed->text(buf, 2, 55);

  oLed->show();
}

void DisplayManager::drawWifiMenu() {
  oLed->fill(0);

  auto *cred = params->credentials;
  if (!cred) {
    oLed->text("No credentials", 2, 24);
    oLed->show();
    return;
  }

  char buf[128];

  snprintf(buf, sizeof(buf), "|%s|", cred->getCurrentFieldName());
  oLed->text(buf, 50, 2);

  snprintf(buf, sizeof(buf), "%s", cred->getCurrentBuffer());
  oLed->text(buf, 0, 12);

  // Get the current charset mode from SetCredentials (the source of truth)
  const auto mode = cred->getCharsetMode();
  const char *charsetName =
      (mode == CharsetMode::LOWERCASE)
        ? "a b c"
        : (mode == CharsetMode::UPPERCASE)
            ? "A B C"
            : (mode == CharsetMode::NUMBERS)
                ? "1 % > _ <"
                : "undef";
  snprintf(buf, sizeof(buf), "Set: %s", charsetName);
  oLed->text(buf, 2, 22);

  char curChar = cred->getCurrentChar();
  snprintf(buf, sizeof(buf), "-- > %c", curChar);
  oLed->text(buf, 55, 32);

  if (unsavedChanges)
    oLed->text("unsaved :[", 50, 56);
  else
    oLed->text("Saved :]", 50, 56);

  oLed->show();
}

void DisplayManager::handleWifiMenuButtons() {
  // FIX #1: Detect actual state change for field switching (debouncing)
  bool currentSSIDSelected = params->inputManager->isSSIDSelected();
  if (currentSSIDSelected != prevSSIDSelected) {
    prevSSIDSelected = currentSSIDSelected;
    params->credentials->nextField();
    unsavedChanges = true;
  }

  // Handle charset mode changes
  auto inputCharset = params->inputManager->getCharsetMode();
  auto credCharset = params->credentials->getCharsetMode();

  // Map from InputManager enum to SetCredentials enum
  CharsetMode targetCharset;
  switch (inputCharset) {
    case InputCharsetMode::CAPITAL:
      targetCharset = CharsetMode::UPPERCASE;
      break;
    case InputCharsetMode::NORMAL:
      targetCharset = CharsetMode::LOWERCASE;
      break;
    case InputCharsetMode::NUMERIC:
      targetCharset = CharsetMode::NUMBERS;
      break;
    default:
      targetCharset = CharsetMode::LOWERCASE;
      break;
  }

  // Only update if the charset has changed
  if (credCharset != targetCharset) {
    // Cycle SetCredentials charset until it matches InputManager's state
    while (params->credentials->getCharsetMode() != targetCharset) {
      params->credentials->nextCharset();
    }
  }

  if (encoder->rotatedCW()) {
    params->credentials->rotateChar(1);
    // Move forward through charset
  }

  if (encoder->rotatedCCW()) {
    params->credentials->rotateChar(-1);
    // Move backward through charset
  }


  if (encoder->buttonPressed()) {
    char selectedChar = params->credentials->getCurrentChar();
    params->credentials->confirmChar();
    unsavedChanges = false; // confirmChar() saves to EEPROM
  }

  // Optional: Handle long-press to delete last character
  if (encoder->buttonHeld()) {
    params->credentials->clearCurrentField();
    unsavedChanges = false;
  }
}

void DisplayManager::changeMenu() {
  menuState = (menuState == MenuState::MAIN)
                ? MenuState::WIFI
                : MenuState::MAIN;
  unsavedChanges = false;
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
