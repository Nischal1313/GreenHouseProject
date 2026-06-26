#pragma once

#include <memory>
#include <cstdio>

#include "debug.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ssd1306os.h"
#include "rotary_encoder.h"
#include "sensor_handler.h"
#include "setCredentials.h"
#include "inputManager.h"

enum class MenuState
{
    MAIN,
    WIFI
};

struct DisplayParams
{
    SensorHandler *pSensorHandlerM;
    SetCredentials *pCredentialsM;
    InputManager *pInputManagerM;
    ssd1306os *pOLedM;
    RotaryEncoder *pEncoderM;
};

class DisplayManager
{
public:
    DisplayManager(std::shared_ptr<Debug> pDebugP,
                   std::shared_ptr<ssd1306os> pOledP,
                   std::shared_ptr<RotaryEncoder> const &pEncoderPtrP);

    void setParams(DisplayParams *pDisplayParamsP);

    // State-machine-driven display rendering
    void drawMainMenu() const;

    void drawWifiMenu();

    void handleWifiMenuButtons();

    // Legacy task entry (not used by state machine supervisor)
    static void taskEntry(void *pvParametersP);

    [[noreturn]] void displayTask();

private:

    void changeMenu();

    [[nodiscard]] MenuState getCurrentMenuState() const;

    std::shared_ptr<ssd1306os> pOLedM;
    std::shared_ptr<Debug> pDebugM;
    std::shared_ptr<RotaryEncoder> pEncoderM;

    DisplayParams *pParamsM;
    MenuState menuStateM;

    bool prevSsidSelectedM;
    bool prevCharsetChangedM;
};
