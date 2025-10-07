// displayMenu.cpp - Fixed version with better save handling and debugging
#include "displayMenu.h"
#include <cstdio>
#include <cstring>

DisplayManager::DisplayManager()
    : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
      oLed(std::make_shared<ssd1306os>(i2cBus)),
      params(nullptr),
      menuState(MenuState::MAIN),
      lastEncoderValue(0),
      unsavedChanges(false)
{
    printf("[DisplayManager] Constructor called\n");
}

void DisplayManager::setParams(DisplayParams* displayParams) {
    this->params = displayParams;
    this->inputManager = displayParams->input;
    this->credentials = displayParams->credentials;
    printf("[DisplayManager] Parameters set\n");
}

void DisplayManager::taskEntry(void *pvParameters) {
    auto *self = static_cast<DisplayManager *>(pvParameters);
    self->displayTask();
}

[[noreturn]] void DisplayManager::displayTask() {
    printf("[DisplayManager] Display task started\n");

    // Periodic auto-save counter
    int autoSaveCounter = 0;
    const int AUTO_SAVE_INTERVAL = 100;  // Save every 5 seconds (100 * 50ms)

    while (true) {
        oLed->fill(0);

        InputManager* input = params->input;
        SetCredentials* cred = params->credentials;

        // Check for MENU button press event (edge-triggered)
        if (input->getMenuPressEvent()) {
            printf("[DisplayManager] Menu button pressed\n");
            const_cast<DisplayManager*>(this)->changeMenu();
        }

        switch (menuState) {
            case MenuState::MAIN:
                const_cast<DisplayManager*>(this)->drawMainMenu();
                break;

            case MenuState::WIFI:
                // Track if we're in WiFi menu for unsaved changes
                bool hadChanges = false;

                // Check for button press events (edge-triggered)
                if (input->getNextFieldPressEvent()) {
                    printf("[DisplayManager] Next field button pressed\n");
                    cred->nextField();  // This already saves the current field
                    hadChanges = true;
                }
                if (input->getCharsetPressEvent()) {
                    printf("[DisplayManager] Charset button pressed\n");
                    cred->nextCharset();
                }

                // Handle encoder rotation for character selection
                int currentEncoderValue = params->encoder->currentRotationValue();
                int encoderDelta = currentEncoderValue - lastEncoderValue;

                // Scale down the encoder delta for smoother rotation
                if (encoderDelta >= 10) {
                    cred->rotateChar(1);
                    lastEncoderValue = currentEncoderValue;
                } else if (encoderDelta <= -10) {
                    cred->rotateChar(-1);
                    lastEncoderValue = currentEncoderValue;
                }

                // Check if character was added (happens in getCurrentBuffer())
                const char* oldBuffer = cred->getCurrentBuffer();
                const_cast<DisplayManager*>(this)->drawWifiMenu();
                const char* newBuffer = cred->getCurrentBuffer();

                if (strcmp(oldBuffer, newBuffer) != 0) {
                    hadChanges = true;
                    printf("[DisplayManager] Buffer changed from '%s' to '%s'\n", oldBuffer, newBuffer);
                }

                if (hadChanges) {
                    unsavedChanges = true;
                }

                // Auto-save periodically when in WiFi menu
                autoSaveCounter++;
                if (unsavedChanges && autoSaveCounter >= AUTO_SAVE_INTERVAL) {
                    printf("[DisplayManager] Auto-saving WiFi credentials (periodic)...\n");
                    cred->saveAllToEEPROM();
                    unsavedChanges = false;
                    autoSaveCounter = 0;
                }
                break;
        }

        oLed->show();
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms for responsive display
    }
}

void DisplayManager::drawMainMenu() {
    char buf[128];
    snprintf(buf, sizeof(buf), "CO2: %.0f ppm", params->gmpSensor->readMeasuredCO2());
    oLed->text(buf, 2, 5);
    snprintf(buf, sizeof(buf), "Set: %d ppm", params->encoder->currentRotationValue());
    oLed->text(buf, 2, 15);
    snprintf(buf, sizeof(buf), "Temp: %.1fC", params->hmpSensor->readTemperature());
    oLed->text(buf, 2, 25);
    snprintf(buf, sizeof(buf), "Hum: %.1f%%", params->hmpSensor->readHumidity());
    oLed->text(buf, 2, 35);
    snprintf(buf, sizeof(buf), "Fan: %.0f%%", params->modbusSystem->readFanSpeed());
    oLed->text(buf, 2, 45);
    snprintf(buf, sizeof(buf), "Valve: %s", params->modbusSystem->valveStatus() ? "OPEN" : "CLOSED");
    oLed->text(buf, 2, 55);
}

void DisplayManager::drawWifiMenu() {
    char buf[128];
    auto* cred = credentials;

    // --- Current field name ---
    snprintf(buf, sizeof(buf), "Field: %s", cred->getCurrentFieldName());
    oLed->text(buf, 2, 0);

    // --- Current buffer value ---
    const char* bufferValue = cred->getCurrentBuffer();
    snprintf(buf, sizeof(buf), "Value: %s", bufferValue);
    oLed->text(buf, 2, 12);

    // --- Current charset display ---
    CharsetMode mode = cred->getCharsetMode();
    const char* charsetName =
        (mode == CharsetMode::LOWERCASE) ? "abc" :
        (mode == CharsetMode::UPPERCASE) ? "ABC" :
        (mode == CharsetMode::NUMBERS) ? "123" :
        "undef";
    snprintf(buf, sizeof(buf), "Charset: %s", charsetName);
    oLed->text(buf, 2, 24);

    // --- Currently selected character ---
    char curChar = cred->getCurrentChar();
    snprintf(buf, sizeof(buf), "Char: [ %c ]", curChar);
    oLed->text(buf, 2, 36);

    // --- Instructions ---
    oLed->text("Turn=Select", 2, 48);

    // Show save status
    if (unsavedChanges) {
        oLed->text("Push=Add *UNSAVED*", 2, 56);
    } else {
        oLed->text("Push=Add Menu=Save", 2, 56);
    }
}

void DisplayManager::changeMenu() {
    if (menuState == MenuState::MAIN) {
        menuState = MenuState::WIFI;
        // Reset encoder value when entering WiFi menu
        lastEncoderValue = params->encoder->currentRotationValue();
        unsavedChanges = false;  // Reset unsaved flag

        printf("\n[DisplayManager] ===== ENTERING WIFI MENU =====\n");
        printf("[DisplayManager] Current SSID: '%s'\n", credentials->getWifiSSID());
        printf("[DisplayManager] Current Password: '%s'\n", credentials->getWifiPassword());
        printf("[DisplayManager] ================================\n\n");
    } else {
        printf("\n[DisplayManager] ===== EXITING WIFI MENU =====\n");
        printf("[DisplayManager] Performing final save before exit...\n");

        // Save when exiting WIFI menu
        credentials->saveAllToEEPROM();

        // Add a delay to ensure EEPROM write completes
        vTaskDelay(pdMS_TO_TICKS(50));

        menuState = MenuState::MAIN;
        unsavedChanges = false;

        printf("[DisplayManager] Final SSID: '%s'\n", credentials->getWifiSSID());
        printf("[DisplayManager] Final Password: '%s'\n", credentials->getWifiPassword());
        printf("[DisplayManager] Switched to MAIN menu, credentials saved\n");
        printf("[DisplayManager] ================================\n\n");
    }
}