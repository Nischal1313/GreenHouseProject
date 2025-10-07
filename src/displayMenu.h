// displayMenu.h - Updated header with save tracking
#pragma once
#include <memory>
#include "PicoI2C.h"
#include "ssd1306os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "rotaryEncoder.h"
#include "inputManager.h"
#include "setCredentials.h"

enum class MenuState {
    MAIN,
    WIFI
};

struct DisplayParams {
    GMP252* gmpSensor;
    HMP60* hmpSensor;
    ModbusMIO* modbusSystem;
    RotaryEncoder* encoder;
    InputManager* input;
    SetCredentials* credentials;
};

class DisplayManager {
public:
    DisplayManager();
    void setParams(DisplayParams* displayParams);
    [[noreturn]] void displayTask();
    static void taskEntry(void *pvParameters);

private:
    std::shared_ptr<PicoI2C> i2cBus;
    std::shared_ptr<ssd1306os> oLed;
    DisplayParams* params;
    InputManager* inputManager;
    SetCredentials* credentials;
    MenuState menuState;
    int lastEncoderValue;
    bool unsavedChanges;  // Track if there are unsaved changes

    void drawMainMenu();
    void drawWifiMenu();
    void changeMenu();
};