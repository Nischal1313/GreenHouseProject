#include "FreeRTOS.h"
#include "task.h"
#include "setCredentials.h"
#include "displayMenu.h"
#include <cstdarg>
#include <cstring>
#include "pico/stdio.h"


DisplayManager::DisplayManager(std::shared_ptr<Debug> debug,
                               std::shared_ptr<ssd1306os> oled,
                               const std::shared_ptr<RotaryEncoder> &encoderPtr)
  : oLed(std::move(oled)),
    debug(std::move(debug)),
    encoder(encoderPtr),
    params(nullptr),
    lastWifiEncoderPos(0),
    unsavedChanges(false),
    menuState(MenuState::MAIN) {
}

void DisplayManager::setParams(DisplayParams *displayParams) {
  if (!displayParams) {
    printf("[DisplayManager] ERROR: displayParams is NULL!\n");
    return;
  }
  params = displayParams;
  printf("[DisplayManager] Parameters set successfully\n");
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

    const bool inMainMenu = params->inputManager->isMainMenu();

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
  const auto [temperature, humidity, co2,
        fanSpeed, valveOpen, targetCo2] =
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

  snprintf(buf, sizeof(buf), "Valve: %s", valveOpen ? "OPEN" : "CLOSED");
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

  snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
  oLed->text(buf, 2, 0);

  snprintf(buf, sizeof(buf), "# %s", cred->getCurrentBuffer());
  oLed->text(buf, 2, 12);

  // Get the current charset mode from SetCredentials (the source of truth)
  const auto mode = cred->getCharsetMode();
  const char *charsetName =
      (mode == CharsetMode::LOWERCASE)
        ? "abc"
        : (mode == CharsetMode::UPPERCASE)
            ? "ABC"
            : (mode == CharsetMode::NUMBERS)
                ? "123"
                : "undef";
  snprintf(buf, sizeof(buf), "Charset: %s", charsetName);
  oLed->text(buf, 2, 24);

  char curChar = cred->getCurrentChar();
  snprintf(buf, sizeof(buf), "Push: %c", curChar);
  oLed->text(buf, 2, 36);

  oLed->text("Turn=Char", 2, 48);

  if (unsavedChanges)
    oLed->text("*UNSAVED*", 2, 56);
  else
    oLed->text("Saved", 2, 56);

  oLed->show();

  printf("[DisplayManager] WiFi menu drawn - Field: %s, Char: %c\n",
         cred->getCurrentFieldName(), curChar);
}

void DisplayManager::handleWifiMenuButtons() {
  // Handle field switching (SSID <-> Password)
  if (params->inputManager->isSSIDSelected()) {
    printf("[DisplayManager] Next Field button pressed\n");
    params->credentials->nextField();
    unsavedChanges = true;
  }

  // Map InputManager's InputCharsetMode to SetCredentials' CharsetMode
  // InputManager: CAPITAL, NORMAL, NUMERIC
  // SetCredentials: UPPERCASE, LOWERCASE, NUMBERS
  const auto inputCharset = params->inputManager->getCharsetMode();
  const auto credCharset = params->credentials->getCharsetMode();

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
    printf("[DisplayManager] Charset mode changed to: %d\n", static_cast<int>(targetCharset));
    // Cycle SetCredentials charset until it matches InputManager's state
    while (params->credentials->getCharsetMode() != targetCharset) {
      params->credentials->nextCharset();
    }
  }
}

void DisplayManager::changeMenu() {
  menuState = (menuState == MenuState::MAIN) ? MenuState::WIFI : MenuState::MAIN;
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
