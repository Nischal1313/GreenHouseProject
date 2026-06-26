//
// Created by Keijo Länsikunnas on 18.2.2024.
// This based on MicroPython modframebuf.c
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
#include <algorithm>
#include "framebuf.h"
#include "font_petme128_8x8.h"

framebuf::framebuf(uint16_t widthP, uint16_t heightP) :
    widthM(widthP), heightM(heightP)
{

}

void framebuf::line(uint16_t x1P, uint16_t y1P, uint16_t x2P, uint16_t y2P, uint32_t colorP)
{
    int dx = static_cast<int>(x2P) - static_cast<int>(x1P);
    int sx;
    if (dx > 0)
    {
        sx = 1;
    }
    else
    {
        dx = -dx;
        sx = -1;
    }

    int dy = static_cast<int>(y2P) - static_cast<int>(y1P);
    int sy;
    if (dy > 0)
    {
        sy = 1;
    }
    else
    {
        dy = -dy;
        sy = -1;
    }

    bool steep;
    int x1 = x1P;
    int y1 = y1P;
    if (dy > dx)
    {
        int temp;
        temp = x1;
        x1 = y1;
        y1 = temp;
        temp = dx;
        dx = dy;
        dy = temp;
        temp = sx;
        sx = sy;
        sy = temp;
        steep = true;
    }
    else
    {
        steep = false;
    }

    int e = 2 * dy - dx;
    for (int i = 0; i < dx; ++i)
    {
        if (steep)
        {
            if (0 <= y1 && y1 < static_cast<int>(widthM) && 0 <= x1 && x1 < static_cast<int>(heightM))
            {
                setpixel(static_cast<uint16_t>(y1), static_cast<uint16_t>(x1), colorP);
            }
        }
        else
        {
            if (0 <= x1 && x1 < static_cast<int>(widthM) && 0 <= y1 && y1 < static_cast<int>(heightM))
            {
                setpixel(static_cast<uint16_t>(x1), static_cast<uint16_t>(y1), colorP);
            }
        }
        while (e >= 0)
        {
            y1 += sy;
            e -= 2 * dx;
        }
        x1 += sx;
        e += 2 * dy;
    }

    int x2 = x2P;
    int y2 = y2P;
    if (0 <= x2 && x2 < static_cast<int>(widthM) && 0 <= y2 && y2 < static_cast<int>(heightM))
    {
        setpixel(static_cast<uint16_t>(x2), static_cast<uint16_t>(y2), colorP);
    }
}

void framebuf::hline(uint16_t xP, uint16_t yP, uint16_t wP, uint32_t colorP)
{
    fill_rect(xP, yP, wP, 1, colorP);
}

void framebuf::vline(uint16_t xP, uint16_t yP, uint16_t hP, uint32_t colorP)
{
    fill_rect(xP, yP, 1, hP, colorP);
}

void framebuf::rect(uint16_t xP, uint16_t yP, uint16_t wP, uint16_t hP, uint32_t colorP, bool fillP)
{
    if (fillP)
    {
        fill_rect(xP, yP, wP, hP, colorP);
    }
    else
    {
        fill_rect(xP, yP, wP, 1, colorP);
        fill_rect(xP, yP + hP - 1, wP, 1, colorP);
        fill_rect(xP, yP, 1, hP, colorP);
        fill_rect(xP + wP - 1, yP, 1, hP, colorP);
    }
}

void framebuf::text(std::string const &rStrP, uint16_t xP, uint16_t yP, uint32_t colorP)
{
    text(rStrP.c_str(), xP, yP, colorP);
}

void framebuf::text(char const *pStrP, uint16_t xP, uint16_t yP, uint32_t colorP)
{
    uint16_t x = xP;
    for (; *pStrP; ++pStrP)
    {
        int chr = *reinterpret_cast<uint8_t const *>(pStrP);
        if (chr < 32 || chr > 127)
        {
            chr = 127;
        }
        uint8_t const *pChrData = &font_petme128_8x8[(chr - 32) * 8];
        for (int j = 0; j < 8; j++, x++)
        {
            if (0 <= x && x < widthM)
            {
                uint32_t vlineData = pChrData[j];
                for (int y1 = yP; vlineData; vlineData >>= 1, y1++)
                {
                    if (vlineData & 1)
                    {
                        if (0 <= y1 && y1 < heightM)
                        {
                            setpixel(x, static_cast<uint16_t>(y1), colorP);
                        }
                    }
                }
            }
        }
    }
}

void framebuf::fill(uint32_t colorP)
{
    fill_rect(0, 0, widthM, heightM, colorP);
}

void framebuf::blit(framebuf &rSourceP, int16_t xP, int16_t yP, uint32_t keyP, framebuf const *pPaletteP)
{
    if (
        (xP >= static_cast<int16_t>(widthM)) ||
        (yP >= static_cast<int16_t>(heightM)) ||
        (-xP >= static_cast<int16_t>(rSourceP.widthM)) ||
        (-yP >= static_cast<int16_t>(rSourceP.heightM))
    )
    {
        // Out of bounds, no-op.
        return;
    }

    // Clip.
    int32_t x0 = std::max(0, static_cast<int>(xP));
    int32_t y0 = std::max(0, static_cast<int>(yP));
    int32_t x1 = std::max(0, static_cast<int>(-xP));
    int32_t y1 = std::max(0, static_cast<int>(-yP));
    int32_t x0end = std::min(static_cast<int32_t>(widthM), static_cast<int32_t>(xP) + static_cast<int32_t>(rSourceP.widthM));
    int32_t y0end = std::min(static_cast<int32_t>(heightM), static_cast<int32_t>(yP) + static_cast<int32_t>(rSourceP.heightM));

    for (; y0 < y0end; ++y0)
    {
        int cx1 = x1;
        for (int cx0 = x0; cx0 < x0end; ++cx0)
        {
            uint32_t col = rSourceP.getpixel(static_cast<uint16_t>(cx1), static_cast<uint16_t>(y1));
            if (pPaletteP)
            {
                col = pPaletteP->getpixel(col, 0);
            }
            if (col != keyP)
            {
                setpixel(static_cast<uint16_t>(cx0), static_cast<uint16_t>(y0), col);
            }
            ++cx1;
        }
        ++y1;
    }
}

void framebuf::scroll(int16_t xStepP, int16_t yStepP)
{
    int sx{};
    int y{};
    int xEnd{};
    int yEnd{};
    int dx{};
    int dy{};
    bool valid{true};

    if (xStepP < 0)
    {
        sx = 0;
        xEnd = static_cast<int>(widthM) + xStepP;
        valid = xEnd > 0;
        if (valid)
        {
            dx = 1;
        }
    }
    else
    {
        sx = static_cast<int>(widthM) - 1;
        xEnd = xStepP - 1;
        valid = xEnd < sx;
        if (valid)
        {
            dx = -1;
        }
    }

    if (valid)
    {
        if (yStepP < 0)
        {
            y = 0;
            yEnd = static_cast<int>(heightM) + yStepP;
            valid = yEnd > 0;
            if (valid)
            {
                dy = 1;
            }
        }
        else
        {
            y = static_cast<int>(heightM) - 1;
            yEnd = yStepP - 1;
            valid = yEnd < y;
            if (valid)
            {
                dy = -1;
            }
        }
    }

    if (valid)
    {
        for (; y != yEnd; y += dy)
        {
            for (int x = sx; x != xEnd; x += dx)
            {
                setpixel(
                    static_cast<uint16_t>(x),
                    static_cast<uint16_t>(y),
                    getpixel(
                        static_cast<uint16_t>(x - xStepP),
                        static_cast<uint16_t>(y - yStepP)
                    )
                );
            }
        }
    }
}
