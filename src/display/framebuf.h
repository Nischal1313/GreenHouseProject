//
// Created by Keijo Länsikunnas on 18.2.2024.
//

#pragma once

#include <string>
#include <cstdint>

class framebuf
{
public:
    framebuf(uint16_t widthP, uint16_t heightP);
    virtual ~framebuf() = default;
    void fill(uint32_t colorP);
    void line(uint16_t x1P, uint16_t y1P, uint16_t x2P, uint16_t y2P, uint32_t colorP);
    void hline(uint16_t xP, uint16_t yP, uint16_t wP, uint32_t colorP);
    void vline(uint16_t xP, uint16_t yP, uint16_t hP, uint32_t colorP);
    void rect(uint16_t xP, uint16_t yP, uint16_t wP, uint16_t hP, uint32_t colorP, bool fillP = false);
    void text(char const *pStrP, uint16_t xP, uint16_t yP, uint32_t colorP = 1);
    void text(std::string const &rStrP, uint16_t xP, uint16_t yP, uint32_t colorP = 1);
    void blit(framebuf &rFbP, int16_t xP, int16_t yP, uint32_t keyP = 0xFFFF, framebuf const *pPaletteP = nullptr);
    void scroll(int16_t xStepP, int16_t yStepP);
private:
    virtual void setpixel(uint16_t xP, uint16_t yP, uint32_t colorP) = 0;
    virtual uint32_t getpixel(uint16_t xP, uint16_t yP) const = 0;
    virtual void fill_rect(uint16_t xP, uint16_t yP, uint16_t wP, uint16_t hP, uint32_t colorP) = 0;
protected:
    uint16_t widthM;
    uint16_t heightM;
};
