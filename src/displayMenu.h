#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <memory>
#include <cstdio>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#include "ssd1306os.h"
#include "PicoI2C.h"
#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "rotaryEncoder.h"

struct DisplayParams {
    std::shared_ptr<PicoI2C> i2cBus;
    std::shared_ptr<ssd1306os> oled;
    GMP252* gmpSensor;
    HMP60* hmpSensor;
    ModbusMIO* modbusSystem;
    RotaryEncoder* encoder;
    RELAYCONTROL* valve;
};

class DisplayManager {
public:
    explicit DisplayManager(const DisplayParams& params);
    [[noreturn]] void displayTask() const;

private:
    std::shared_ptr<PicoI2C> i2cBus;
    std::shared_ptr<ssd1306os> oled;
    GMP252* gmpSensor;
    HMP60* hmpSensor;
    ModbusMIO* modbusSystem;
    RotaryEncoder* encoder;
    RELAYCONTROL* valve;
};

#endif
