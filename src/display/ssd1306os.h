//
// Created by Keijo Länsikunnas on 16.9.2024.
//

#pragma once

#include <memory>

#include "mono_vlsb.h"
#include "PicoI2C.h"

class ssd1306os : public mono_vlsb
{
public:
    explicit ssd1306os(std::shared_ptr<PicoI2C> pI2cP, uint16_t deviceAddressP = 0x3C, uint16_t widthP = 128, uint16_t heightP = 64);
    void show();
private:
    void init();
    void sendCmd(uint8_t valueP);
    std::shared_ptr<PicoI2C> pSsd1306I2cM;
    uint8_t addressM;
};
