//
// Created by Keijo Länsikunnas on 18.2.2024.
//

#pragma once

#include "mono_vlsb.h"
#include "hardware/i2c.h"

class ssd1306 : public mono_vlsb
{
public:
    explicit ssd1306(i2c_inst *pI2cP, uint16_t deviceAddressP = 0x3C, uint16_t widthP = 128, uint16_t heightP = 64);
    void show();
private:
    void init();
    void sendCmd(uint8_t valueP);
    i2c_inst *pSsd1306I2cM;
    uint8_t addressM;
};
