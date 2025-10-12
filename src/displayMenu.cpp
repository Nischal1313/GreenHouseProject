#include "displayMenu.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>
#include "pico/stdio.h"

DisplayManager::DisplayManager(std::shared_ptr<Debug> debug, std::shared_ptr<ssd1306os> oled)
  : oLed(std::move(oled)),
    params(nullptr),
    menuState(MenuState::MAIN),
    lastEncoderValue(0),
    unsavedChanges(false),
    debug(std::move(debug)) {
}
void DisplayManager::setParams(DisplayParams *displayParams) {
  printf("in here\n");

  if (!displayParams || !displayParams->sensorHandler) {
    printf("[DisplayManager] ERROR: Missing sensorHandler or displayParams!\n");
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
  }

  params = displayParams;
  lastEncoderValue = params->encoder->getPosition();
  printf("[DisplayManager] Parameters set\n");
}

[[noreturn]] void DisplayManager::displayTask() {
  log("[DisplayManager] Display task started\n");
  int autoSaveCounter = 0;
  int idleUpdateCounter = 0;
  int lastEncoderValueLocal = lastEncoderValue;

  while (true) {
    oLed->fill(0);

    // Handle MENU button press (switch between MAIN and WIFI menus)
    if (params->inputManager->getMenuPressEvent()) {
      log("[DisplayManager] Menu button pressed\n");
      changeMenu();
    }

    // Handle WiFi menu specific buttons
    if (menuState == MenuState::WIFI) {
      handleWifiMenuButtons();
    }

    // Read encoder rotation
    const int encoderValue = params->encoder->getPosition();
    const int encoderDelta = encoderValue - lastEncoderValueLocal;

    bool displayNeedsUpdate = false;

    if (encoderDelta != 0) {
      // Encoder rotated: update setpoint and display immediately
      lastEncoderValueLocal = encoderValue;
      if (menuState == MenuState::MAIN) {
        // Update CO2 setpoint in main menu
        params->setpointManager->updateLocal(
          encoderValue,
          SetpointManager::MenuState::MAIN
        );
      } else if (menuState == MenuState::WIFI) {
        // Rotate through characters in WiFi menu
        params->credentials->rotateChar(encoderDelta);
        unsavedChanges = true;
      }
      displayNeedsUpdate = true;
      idleUpdateCounter = 0; // reset idle counter
    } else {
      // Encoder idle: update display only periodically
      idleUpdateCounter++;
      if (idleUpdateCounter * 50 >= 1000) {
        // Update every 1 second when idle
        displayNeedsUpdate = true;
        idleUpdateCounter = 0;
      }
    }

    if (displayNeedsUpdate) {
      switch (menuState) {
        case MenuState::MAIN:
          drawMainMenu();
          break;
        case MenuState::WIFI:
          drawWifiMenu();
          break;
      }
    }

    // Auto-save WiFi credentials periodically
    if (menuState == MenuState::WIFI) {
      autoSaveCounter += 50; // increment by task delay
      if (unsavedChanges && autoSaveCounter >= AUTO_SAVE_INTERVAL_MS) {
        log("[DisplayManager] Auto-saving WiFi credentials\n");
        params->credentials->saveAllToEEPROM();
        unsavedChanges = false;
        autoSaveCounter = 0;
      }
    } else {
      autoSaveCounter = 0;
    }

    oLed->show();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void DisplayManager::handleWifiMenuButtons() {
  // NEXT FIELD button - move to next field (SSID/Password)
  if (params->inputManager->getNextFieldPressEvent()) {
    log("[DisplayManager] Next Field button pressed\n");
    params->credentials->nextField();
    unsavedChanges = true;
  }

  // CHARSET button - cycle through character sets (abc/ABC/123)
  if (params->inputManager->getCharsetPressEvent()) {
    log("[DisplayManager] Charset button pressed\n");
    params->credentials->nextCharset();
  }
}

void DisplayManager::drawMainMenu() const {
  char buf[128];

  auto [temperature, humidity, co2, fanSpeed, valveOn] = params->sensorHandler->getLatestReadings();
  const int setpoint = params->setpointManager->getEffectiveTarget();

  snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
  oLed->text(buf, 2, 5);

  snprintf(buf, sizeof(buf), "Set: %d ppm", setpoint);
  oLed->text(buf, 2, 15);

  snprintf(buf, sizeof(buf), "Temp: %.1fC", temperature);
  oLed->text(buf, 2, 25);

  snprintf(buf, sizeof(buf), "Hum: %.1f%%", humidity);
  oLed->text(buf, 2, 35);

  snprintf(buf, sizeof(buf), "Fan: %.0f%%", fanSpeed);
  oLed->text(buf, 2, 45);

  snprintf(buf, sizeof(buf), "Valve: %s", valveOn ? "OPEN" : "CLOSED");
  oLed->text(buf, 2, 55);
}

void DisplayManager::drawWifiMenu() {
  auto *cred = params->credentials;
  char buf[128];

  snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
  oLed->text(buf, 2, 0);

  snprintf(buf, sizeof(buf), "# %s", cred->getCurrentBuffer());
  oLed->text(buf, 2, 12);

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

  char curChar = cred->getCurrentChar();
  snprintf(buf, sizeof(buf), "Push: %c", curChar);
  oLed->text(buf, 2, 36);

  oLed->text("Turn=Char", 2, 48);

  if (unsavedChanges)
    oLed->text("*UNSAVED*", 2, 56);
  else
    oLed->text("Saved", 2, 56);
}

void DisplayManager::changeMenu() {
  if (menuState == MenuState::MAIN) {
    menuState = MenuState::WIFI;
    lastEncoderValue = params->encoder->getPosition();
    unsavedChanges = false;
    log("\n[DisplayManager] Entering WIFI menu\nSSID='%s' PW='%s'\n",
        params->credentials->getWifiSSID(),
        params->credentials->getWifiPassword());
  } else {
    log("\n[DisplayManager] Exiting WIFI menu\nSaving credentials...\n");
    params->credentials->saveAllToEEPROM();
    vTaskDelay(pdMS_TO_TICKS(50));
    menuState = MenuState::MAIN;
    unsavedChanges = false;
    log("[DisplayManager] Switched to MAIN menu\nSSID='%s' PW='%s'\n",
        params->credentials->getWifiSSID(),
        params->credentials->getWifiPassword());
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
