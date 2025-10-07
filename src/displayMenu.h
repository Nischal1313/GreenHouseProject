
// displayMenu.h - Updated header (minor change)
#ifndef DISPLAY_MENU
#define DISPLAY_MENU

#include <memory>
#include "FreeRTOS.h"
#include "task.h"
#include "ssd1306os.h"
#include "PicoI2C.h"
#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "rotaryEncoder.h"
#include "relayController.h"
#include "inputManager.h"
#include "setCredentials.h"

struct DisplayParams {
    GMP252 *gmpSensor;
    HMP60 *hmpSensor;
    ModbusMIO *modbusSystem;
    RotaryEncoder *encoder;
    InputManager *input;
    SetCredentials *credentials;
};

enum class MenuState {
    MAIN,
    WIFI
};

class DisplayManager {
public:
    DisplayManager();

    void setParams(DisplayParams *displayParams);
    [[noreturn]] void displayTask();
    static void taskEntry(void *pvParameters);

    void drawMainMenu();
    void drawWifiMenu();
    void changeMenu();

private:
    std::shared_ptr<PicoI2C> i2cBus;
    std::shared_ptr<ssd1306os> oLed;
    DisplayParams *params;
    InputManager* inputManager = nullptr;
    SetCredentials* credentials = nullptr;
    MenuState menuState = MenuState::MAIN;
    int lastEncoderValue = 0;
    // Removed lastMenuState - no longer needed with event-based handling
};

#endif