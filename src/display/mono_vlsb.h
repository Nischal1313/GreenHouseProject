//
// Created by Keijo Länsikunnas on 18.2.2024.
//

#pragma once
#include <memory>
#include "framebuf.h"

class mono_vlsb : public framebuf
{
public:
    mono_vlsb(uint16_t widthP, uint16_t heightP, uint16_t strideP = 0, uint16_t bufOffsetP = 0);
    mono_vlsb(uint8_t const *pImageP, uint8_t widthP, uint16_t heightP, uint16_t strideP = 0, uint16_t bufOffsetP = 0);
private:
    void setpixel(uint16_t xP, uint16_t yP, uint32_t colorP) override;
    uint32_t getpixel(uint16_t xP, uint16_t yP) const override;
    void fill_rect(uint16_t xP, uint16_t yP, uint16_t wP, uint16_t hP, uint32_t colorP) override;
protected:
    uint32_t sizeM;
    uint16_t strideM;
    uint16_t bufferOffsetM;
    std::shared_ptr<uint8_t> bufferM;
};
