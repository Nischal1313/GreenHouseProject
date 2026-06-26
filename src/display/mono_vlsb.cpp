//
// Created by Keijo Länsikunnas on 18.2.2024.
//
// This is based on MicroPython modframebuf.c
// modframebuf.c licence attached below
/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <cstring>
#include "mono_vlsb.h"

mono_vlsb::mono_vlsb(uint16_t widthP, uint16_t heightP, uint16_t strideP, uint16_t bufOffsetP) :
    framebuf(widthP, heightP),
    sizeM(static_cast<uint32_t>(widthP) * (heightP / 8 + (heightP % 8 ? 1 : 0)) + bufOffsetP),
    strideM(strideP),
    bufferOffsetM(bufOffsetP),
    bufferM(std::shared_ptr<uint8_t>(new uint8_t[sizeM]))
{
    // zero out the buffer
    std::memset(bufferM.get(), 0, sizeM);
    if (strideM < widthP)
    {
        strideM = widthP;
    }
}

mono_vlsb::mono_vlsb(uint8_t const *pImageP, uint8_t widthP, uint16_t heightP, uint16_t strideP, uint16_t bufOffsetP) :
    framebuf(widthP, heightP),
    sizeM(static_cast<uint32_t>(widthP) * (heightP / 8 + (heightP % 8 ? 1 : 0)) + bufOffsetP),
    strideM(strideP),
    bufferOffsetM(bufOffsetP),
    bufferM(std::shared_ptr<uint8_t>(new uint8_t[sizeM]))
{
    // copy image to the buffer
    std::memcpy(bufferM.get() + bufOffsetP, pImageP, sizeM - bufOffsetP);
    if (strideM < widthM)
    {
        strideM = widthP;
    }
}

void mono_vlsb::setpixel(uint16_t xP, uint16_t yP, uint32_t colorP)
{
    size_t index = static_cast<size_t>(yP >> 3) * strideM + xP + bufferOffsetM;
    uint8_t offset = yP & 0x07;
    bufferM.get()[index] = (bufferM.get()[index] & ~(0x01 << offset)) | ((colorP != 0) << offset);
}

uint32_t mono_vlsb::getpixel(uint16_t xP, uint16_t yP) const
{
    return (bufferM.get()[static_cast<size_t>(yP >> 3) * strideM + xP + bufferOffsetM] >> (yP & 0x07)) & 0x01;
}

void mono_vlsb::fill_rect(uint16_t xP, uint16_t yP, uint16_t wP, uint16_t hP, uint32_t colorP)
{
    uint16_t y = yP;
    uint16_t h = hP;
    while (h--)
    {
        uint8_t *pB = &bufferM.get()[static_cast<size_t>(y >> 3) * strideM + xP + bufferOffsetM];
        uint8_t offset = y & 0x07;
        for (unsigned int ww = wP; ww; --ww)
        {
            *pB = (*pB & ~(0x01 << offset)) | ((colorP != 0) << offset);
            ++pB;
        }
        ++y;
    }
}
