// DisplayHandler.cpp
#include "displayHandler.h"
#include <cstdio>


DisplayHandler::DisplayHandler() : i2c(nullptr), display(nullptr) {}

void DisplayHandler::init() {
    i2c = std::make_shared<PicoI2C>(1, 400000);
    display = new ssd1306os(i2c);
}

void DisplayHandler::showValue(const int32_t value) const {
    display->fill(0);
    char buf[16];
    snprintf(buf, sizeof(buf), "C02 set: %ld", value);
    display->text(buf, 0, 50, 1);
    display->show();
}
