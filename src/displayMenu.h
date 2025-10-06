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
#include "pico/stdio.h"

struct DisplayParams {
    GMP252* gmpSensor;
    HMP60* hmpSensor;
    ModbusMIO* modbusSystem;
    RotaryEncoder* encoder;
    RELAYCONTROL* valve;
};

class DisplayManager {
public:
    DisplayManager();
    void setParams(DisplayParams* displayParams);

    // This function runs inside a FreeRTOS task
    [[noreturn]] void displayTask() const;

    // Static entry point for FreeRTOS
    static void taskEntry(void* pvParameters);

private:
    std::shared_ptr<PicoI2C> i2cBus;
    std::shared_ptr<ssd1306os> oLed;
    DisplayParams* params;
};

#endif
