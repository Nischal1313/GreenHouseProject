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

  printf("[DisplayManager] Setting params:\n");
  printf("  - sensorHandler: %p\n", (void *) displayParams->sensorHandler);
  printf("  - setpointManager: %p\n", (void *) displayParams->setpointManager);
  printf("  - credentials: %p\n", (void *) displayParams->credentials);
  printf("  - inputManager: %p\n", (void *) displayParams->inputManager);
  printf("  - oLed: %p\n", (void *) displayParams->oLed);
  printf("  - encoder: %p\n", (void *) displayParams->encoder);

  params = displayParams;
  printf("[DisplayManager] Parameters set successfully\n");
}

[[noreturn]] void DisplayManager::displayTask() {
  printf("[DisplayManager] displayTask() started\n");

  // Initial safety check
  if (!params) {
    printf("[DisplayManager] FATAL: params is NULL in displayTask!\n");
    while (true) {
      printf("[DisplayManager] Waiting for params...\n");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // Verify all components are valid
  if (!params->sensorHandler || !params->setpointManager ||
      !params->credentials || !params->inputManager ||
      !params->encoder || !oLed) {
    printf("[DisplayManager] ERROR: One or more components are NULL!\n");
    printf("  sensorHandler: %p\n", (void *) params->sensorHandler);
    printf("  setpointManager: %p\n", (void *) params->setpointManager);
    printf("  credentials: %p\n", (void *) params->credentials);
    printf("  inputManager: %p\n", (void *) params->inputManager);
    printf("  encoder: %p\n", (void *) params->encoder);
    printf("  oLed: %p\n", (void *) oLed.get());
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
  }

  log("[DisplayManager] Display task started successfully\n");
  printf("[DisplayManager] Initial menu state: %s\n",
         menuState == MenuState::MAIN ? "MAIN" : "WIFI");

  int autoSaveCounter = 0;
  int updateCounter = 0;
  uint32_t loopCount = 0;

  // Initialize display
  oLed->fill(0);
  oLed->text("CO2 System", 20, 20);
  oLed->text("Starting...", 20, 35);
  oLed->show();
  vTaskDelay(pdMS_TO_TICKS(1000));

  while (true) {
    // Periodic heartbeat
    if (loopCount % 100 == 0) {
      printf("[DisplayManager] Loop %lu, menu=%s\n",
             loopCount, menuState == MenuState::MAIN ? "MAIN" : "WIFI");
    }
    loopCount++;

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

      // ONLY read encoder in WiFi menu - DO NOT read in MAIN menu!
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

    // Update display every second (20 * 50ms = 1000ms)
    updateCounter++;
    if (updateCounter >= 20) {
      printf("[DisplayManager] Updating display for menu: %s\n",
             menuState == MenuState::MAIN ? "MAIN" : "WIFI");

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

    // Auto-save WiFi credentials periodically
    if (menuState == MenuState::WIFI) {
      autoSaveCounter += 50;
      if (unsavedChanges && autoSaveCounter >= AUTO_SAVE_INTERVAL_MS) {
        log("[DisplayManager] Auto-saving WiFi credentials\n");
        params->credentials->saveAllToEEPROM();
        unsavedChanges = false;
        autoSaveCounter = 0;
      }
    } else {
      autoSaveCounter = 0;
    }


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
  printf("[DisplayManager] Drawing MAIN menu\n");

  char buf[128];

  // Get sensor readings
  SensorValues readings = params->sensorHandler->getLatestReadings();
  printf("[DisplayManager] Sensor readings: CO2=%.0f, Temp=%.1f, Hum=%.1f, Fan=%.0f%%\n",
         readings.co2, readings.temperature, readings.humidity, readings.fanSpeed);

  // Get current setpoint
  int setpoint = params->setpointManager->getEffectiveTarget();
  printf("[DisplayManager] Current setpoint: %d ppm\n", setpoint);

  // Clear display
  oLed->fill(0);

  // Draw menu content
  snprintf(buf, sizeof(buf), "CO2: %.0f ppm", readings.co2);
  oLed->text(buf, 2, 5);

  snprintf(buf, sizeof(buf), "Set: %d ppm", setpoint);
  oLed->text(buf, 2, 15);

  snprintf(buf, sizeof(buf), "Temp: %.1fC", readings.temperature);
  oLed->text(buf, 2, 25);

  snprintf(buf, sizeof(buf), "Hum: %.1f%%", readings.humidity);
  oLed->text(buf, 2, 35);

  snprintf(buf, sizeof(buf), "Fan: %.0f%%", readings.fanSpeed);
  oLed->text(buf, 2, 45);

  snprintf(buf, sizeof(buf), "Valve: %s", readings.valveOpen ? "OPEN" : "CLOSED");
  oLed->text(buf, 2, 55);
}

void DisplayManager::drawWifiMenu() {
  printf("[DisplayManager] Drawing WIFI menu\n");

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
    printf("[DisplayManager] ==> Switched to WIFI menu\n");
    log("\n[DisplayManager] Entering WIFI menu\nSSID='%s' PW='%s'\n",
        params->credentials->getWifiSSID(),
        params->credentials->getWifiPassword());
  } else {
    printf("[DisplayManager] ==> Switching to MAIN menu, saving credentials first...\n");
    log("\n[DisplayManager] Exiting WIFI menu\nSaving credentials...\n");
    params->credentials->saveAllToEEPROM();
    vTaskDelay(pdMS_TO_TICKS(50));
    menuState = MenuState::MAIN;
    unsavedChanges = false;
    printf("[DisplayManager] ==> Switched to MAIN menu\n");
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
  vTaskDelay(pdMS_TO_TICKS(500)); // Wait for system to stabilize
  printf("[DisplayManager] Task entry, starting display task...\n");
  self->displayTask();
}
