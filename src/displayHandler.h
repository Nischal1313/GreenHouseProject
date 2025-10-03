// DisplayHandler.h
#pragma once
#include <memory>
#include "ssd1306os.h"
#include "PicoI2C.h"

class DisplayHandler {
public:
    DisplayHandler();
    void init();
    void showValue(int32_t value) const;
private:
    std::shared_ptr<PicoI2C> i2c;
    ssd1306os* display;
};
