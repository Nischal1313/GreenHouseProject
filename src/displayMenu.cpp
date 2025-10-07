// displayMenu.cpp - Fixed version using event-based button handling
#include "displayMenu.h"
#include <cstdio>
#include <cstring>

DisplayManager::DisplayManager()
    : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
      oLed(std::make_shared<ssd1306os>(i2cBus)),
      params(nullptr),
      menuState(MenuState::MAIN),
      lastEncoderValue(0)
{
}

void DisplayManager::setParams(DisplayParams* displayParams) {
    this->params = displayParams;
    this->inputManager = displayParams->input;
    this->credentials = displayParams->credentials;
}

void DisplayManager::taskEntry(void *pvParameters) {
    auto *self = static_cast<DisplayManager *>(pvParameters);
    self->displayTask();
}

[[noreturn]] void DisplayManager::displayTask() {
    while (true) {
        oLed->fill(0);

        InputManager* input = params->input;
        SetCredentials* cred = params->credentials;

        // Check for MENU button press event (edge-triggered)
        if (input->getMenuPressEvent()) {
            const_cast<DisplayManager*>(this)->changeMenu();
        }

        switch (menuState) {
            case MenuState::MAIN:
                const_cast<DisplayManager*>(this)->drawMainMenu();
                break;

            case MenuState::WIFI:
                // Check for button press events (edge-triggered)
                if (input->getNextFieldPressEvent()) {
                    cred->nextField();
                }
                if (input->getCharsetPressEvent()) {
                    cred->nextCharset();
                }
                const_cast<DisplayManager*>(this)->drawWifiMenu();
                break;
        }

        oLed->show();
        vTaskDelay(pdMS_TO_TICKS(50));  // Reduced to 50ms for more responsive display
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
    oLed->text("Push=Add Menu=Save", 2, 56);

    // --- Handle encoder rotation ---
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
}

void DisplayManager::changeMenu() {
    if (menuState == MenuState::MAIN) {
        menuState = MenuState::WIFI;
        // Reset encoder value when entering WiFi menu
        lastEncoderValue = params->encoder->currentRotationValue();
        printf("[DisplayManager] Switched to WIFI menu\n");
    } else {
        // Save when exiting WIFI menu
        credentials->saveAllToEEPROM();
        menuState = MenuState::MAIN;
        printf("[DisplayManager] Switched to MAIN menu, saved credentials\n");
    }
}
