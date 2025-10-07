// #include "displayMenu.h"
// #include <cstdio>
//
// DisplayManager::DisplayManager()
//     : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
//       oLed(std::make_shared<ssd1306os>(i2cBus)),
//       params(nullptr),
//       menuState(MenuState::MAIN)// start at main screen
// {
// }
//
// void DisplayManager::setParams(DisplayParams* displayParams) {
//     this->params = displayParams;
//     this->inputManager = displayParams->input;
//     this->credentials = displayParams->credentials;
// }
//
//
// void DisplayManager::taskEntry(void *pvParameters) {
//     auto *self = static_cast<DisplayManager *>(pvParameters);
//     self->displayTask();
// }
//
// [[noreturn]] void DisplayManager::displayTask() const {
//     while (true) {
//         oLed->fill(0);
//
//         // --- Handle input buttons ---
//         InputManager *input = params->input;
//         SetCredentials *cred = params->credentials;
//
//         if (input->isMenuPressed()) {
//             const_cast<DisplayManager*>(this)->changeMenu();
//         }
//
//         switch (menuState) {
//             case MenuState::MAIN:
//                 const_cast<DisplayManager*>(this)->drawMainMenu();
//                 break;
//
//             case MenuState::WIFI:
//                 if (input->isNextFieldPressed()) {
//                     cred->nextField();
//                 }
//                 if (input->isCharsetPressed()) {
//                     cred->nextCharset();
//                 }
//                 const_cast<DisplayManager*>(this)->drawWifiMenu();
//                 break;
//         }
//
//         oLed->show();
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }
//
// void DisplayManager::drawMainMenu() {
//     char buf[128];
//
//     const float co2 = params->gmpSensor->readMeasuredCO2();
//     const float temp = params->hmpSensor->readTemperature();
//     const float hum = params->hmpSensor->readHumidity();
//     const int desiredCO2 = params->encoder->currentRotationValue();
//     const float fanSpeed = params->modbusSystem->readFanSpeed();
//     const bool valveState = params->modbusSystem->valveStatus();
//
//     snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
//     oLed->text(buf, 2, 5);
//     snprintf(buf, sizeof(buf), "Set: %d ppm", desiredCO2);
//     oLed->text(buf, 2, 15);
//     snprintf(buf, sizeof(buf), "Temp: %.1fC", temp);
//     oLed->text(buf, 2, 25);
//     snprintf(buf, sizeof(buf), "Hum: %.1f%%", hum);
//     oLed->text(buf, 2, 35);
//     snprintf(buf, sizeof(buf), "Fan: %.0f%%", fanSpeed);
//     oLed->text(buf, 2, 45);
//     snprintf(buf, sizeof(buf), "Valve: %s", valveState ? "OPEN" : "CLOSED");
//     oLed->text(buf, 2, 55);
// }
//
// void DisplayManager::drawWifiMenu() {
//     char buf[128];
//     auto* cred = credentials;
//
//     // --- Current field name ---
//     snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
//     oLed->text(buf, 2, 0);
//
//     // --- Current buffer contents ---
//     snprintf(buf, sizeof(buf), "Value: %s", cred->getCurrentBuffer());
//     oLed->text(buf, 2, 12);
//
//     // --- Current charset display ---
//     CharsetMode mode = cred->getCharsetMode();
//     const char* charsetName =
//         (mode == CharsetMode::LOWERCASE) ? "abc" :
//         (mode == CharsetMode::UPPERCASE) ? "ABC" :
//         (mode == CharsetMode::NUMBERS) ? "135" :
//         "undef";
//     snprintf(buf, sizeof(buf), "Charset: %s", charsetName);
//     oLed->text(buf, 2, 24);
//
//     // --- Currently selected character ---
//     char curChar = cred->getCurrentChar();
//     snprintf(buf, sizeof(buf), "[%c]", curChar);
//     oLed->text(buf, 2, 36);
//
//     // --- Instructions ---
//     oLed->text(" Back=Save&Exit", 2, 48);
//
//     oLed->show();
//
//     // --- Handle input ---
//     if (menuState == MenuState::WIFI) {
//         // Compute encoder delta
//         int encoderDelta = params->encoder->currentRotationValue() - lastEncoderValue;
//         encoderDelta /= 10; // scale down to ±1 step
//         lastEncoderValue = params->encoder->currentRotationValue();
//
//         if (encoderDelta != 0) {
//             cred->rotateChar(encoderDelta);
//         }
//
//         if (inputManager->isMenuPressed()) {
//             cred->confirmChar();
//         }
//
//         if (inputManager->isNextFieldPressed()) {
//             cred->nextField();
//         }
//
//         if (inputManager->isCharsetPressed()) {
//             cred->nextCharset();
//         }
//     }
// }
//
//
// void DisplayManager::changeMenu() {
//     if (menuState == MenuState::MAIN)
//         menuState = MenuState::WIFI;
//     else
//         menuState = MenuState::MAIN;
// }
//
#include "displayMenu.h"
#include <cstdio>

DisplayManager::DisplayManager()
    : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
      oLed(std::make_shared<ssd1306os>(i2cBus)),
      params(nullptr),
      menuState(MenuState::MAIN),
      lastMenuState(false),
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

        // --- Debounce MENU button ---
        bool menuPressed = input->isMenuPressed();
        if (menuPressed && !lastMenuState) {
            const_cast<DisplayManager*>(this)->changeMenu();
        }
        lastMenuState = menuPressed;

        switch (menuState) {
            case MenuState::MAIN:
                const_cast<DisplayManager*>(this)->drawMainMenu();
                break;

            case MenuState::WIFI:
                if (input->isNextFieldPressed()) {
                    cred->nextField();
                }
                if (input->isCharsetPressed()) {
                    cred->nextCharset();
                }
                const_cast<DisplayManager*>(this)->drawWifiMenu();
                break;
        }

        oLed->show();
        vTaskDelay(pdMS_TO_TICKS(100));
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

    // --- Current field ---
    snprintf(buf, sizeof(buf), "%s", cred->getCurrentFieldName());
    oLed->text(buf, 2, 0);

    // --- Current buffer ---
    snprintf(buf, sizeof(buf), "Value: %s", cred->getCurrentBuffer());
    oLed->text(buf, 2, 12);

    // --- Charset ---
    CharsetMode mode = cred->getCharsetMode();
    const char* charsetName =
        (mode == CharsetMode::LOWERCASE) ? "abc" :
        (mode == CharsetMode::UPPERCASE) ? "ABC" :
        (mode == CharsetMode::NUMBERS) ? "135" :
        "undef";
    snprintf(buf, sizeof(buf), "Charset: %s", charsetName);
    oLed->text(buf, 2, 24);

    // --- Selected character safely ---
    char curChar = cred->getCurrentChar();
    snprintf(buf, sizeof(buf), "[%c]", curChar);
    oLed->text(buf, 2, 36);

    oLed->text(" Back=Save&Exit", 2, 48);

    // --- Handle encoder rotation ---
    int encoderDelta = params->encoder->currentRotationValue() - lastEncoderValue;
    encoderDelta /= 10; // scale down
    lastEncoderValue = params->encoder->currentRotationValue();

    if (encoderDelta != 0) {
        cred->rotateChar(encoderDelta);
    }

    // --- Handle button presses safely ---
    if (params->input->isMenuPressed()) {
        cred->confirmChar();
    }
    if (params->input->isNextFieldPressed()) {
        cred->nextField();
    }
    if (params->input->isCharsetPressed()) {
        cred->nextCharset();
    }
}

void DisplayManager::changeMenu() {
    if (menuState == MenuState::MAIN)
        menuState = MenuState::WIFI;
    else
        menuState = MenuState::MAIN;
}
