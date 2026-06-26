//
// Created by Keijo Länsikunnas on 16.9.2024.
//

#include "ssd1306os.h"

// commands (see datasheet)
namespace
{
    constexpr uint8_t SSD1306_SET_MEM_MODE = 0x20;
    constexpr uint8_t SSD1306_SET_COL_ADDR = 0x21;
    constexpr uint8_t SSD1306_SET_PAGE_ADDR = 0x22;
    constexpr uint8_t SSD1306_SET_HORIZ_SCROLL = 0x26;
    constexpr uint8_t SSD1306_SET_SCROLL = 0x2E;

    constexpr uint8_t SSD1306_SET_DISP_START_LINE = 0x40;

    constexpr uint8_t SSD1306_SET_CONTRAST = 0x81;
    constexpr uint8_t SSD1306_SET_CHARGE_PUMP = 0x8D;

    constexpr uint8_t SSD1306_SET_SEG_REMAP = 0xA0;
    constexpr uint8_t SSD1306_SET_ENTIRE_ON = 0xA4;
    constexpr uint8_t SSD1306_SET_ALL_ON = 0xA5;
    constexpr uint8_t SSD1306_SET_NORM_DISP = 0xA6;
    constexpr uint8_t SSD1306_SET_INV_DISP = 0xA7;
    constexpr uint8_t SSD1306_SET_MUX_RATIO = 0xA8;
    constexpr uint8_t SSD1306_SET_DISP = 0xAE;
    constexpr uint8_t SSD1306_SET_COM_OUT_DIR = 0xC0;
    constexpr uint8_t SSD1306_SET_COM_OUT_DIR_FLIP = 0xC0;

    constexpr uint8_t SSD1306_SET_DISP_OFFSET = 0xD3;
    constexpr uint8_t SSD1306_SET_DISP_CLK_DIV = 0xD5;
    constexpr uint8_t SSD1306_SET_PRECHARGE = 0xD9;
    constexpr uint8_t SSD1306_SET_COM_PIN_CFG = 0xDA;
    constexpr uint8_t SSD1306_SET_VCOM_DESEL = 0xDB;

    constexpr uint8_t SSD1306_PAGE_HEIGHT = 8;

    constexpr uint8_t SSD1306_WRITE_MODE = 0xFE;
    constexpr uint8_t SSD1306_READ_MODE = 0xFF;
}

/* Constructor allocates buffer that is one bigger than what is needed.
 * The extra byte is needed for the command byte when updating the display.
 * Height must be multiple of 8.
 */
ssd1306os::ssd1306os(std::shared_ptr<PicoI2C> pI2cP, uint16_t deviceAddressP, uint16_t widthP, uint16_t heightP) :
    mono_vlsb(widthP, heightP, widthP, 1),
    pSsd1306I2cM(pI2cP), addressM(static_cast<uint8_t>(deviceAddressP))
{
    // set control byte at the beginning of frame buffer
    bufferM.get()[0] = 0x40;
    init();
}

void ssd1306os::init()
{
    // Some of these commands are not strictly necessary as the reset
    // process defaults to some of these but they are shown here
    // to demonstrate what the initialization sequence looks like
    // Some configuration values are recommended by the board manufacturer

    uint8_t cmds[] = {
            SSD1306_SET_DISP,               // set display off
            /* memory mapping */
            SSD1306_SET_MEM_MODE,           // set memory address mode 0 = horizontal, 1 = vertical, 2 = page
            0x00,                           // horizontal addressing mode
            /* resolution and layout */
            SSD1306_SET_DISP_START_LINE,    // set display start line to 0
            SSD1306_SET_SEG_REMAP | 0x01,   // set segment re-map, column address 127 is mapped to SEG0
            SSD1306_SET_MUX_RATIO,          // set multiplex ratio
            uint8_t(heightM - 1),             // Display height - 1
            SSD1306_SET_COM_OUT_DIR |
            0x08, // set COM (common) output scan direction. Scan from bottom up, COM[N-1] to COM0
            SSD1306_SET_DISP_OFFSET,        // set display offset
            0x00,                           // no offset
            SSD1306_SET_COM_PIN_CFG,        // set COM (common) pins hardware configuration. Board specific magic number.
            // 0x02 Works for 128x32, 0x12 Possibly works for 128x64. Other options 0x22, 0x32
            0x02, // this is changed later in the constructor if display size is not 128x32

            /* timing and driving scheme */
            SSD1306_SET_DISP_CLK_DIV,       // set display clock divide ratio
            0x80,                           // div ratio of 1, standard freq
            SSD1306_SET_PRECHARGE,          // set pre-charge period
            0xF1,                           // Vcc internally generated on our board
            SSD1306_SET_VCOM_DESEL,         // set VCOMH deselect level
            0x30,                           // 0.83xVcc
            /* display */
            SSD1306_SET_CONTRAST,           // set contrast control
            0xFF,
            SSD1306_SET_ENTIRE_ON,          // set entire display on to follow RAM content
            SSD1306_SET_NORM_DISP,           // set normal (not inverted) display
            SSD1306_SET_CHARGE_PUMP,        // set charge pump
            0x14,                           // Vcc internally generated on our board
            SSD1306_SET_SCROLL |
            0x00,      // deactivate horizontal scrolling if set. This is necessary as memory writes will corrupt if scrolling was enabled
            SSD1306_SET_DISP | 0x01, // turn display on
    };
    if (heightM > 32)
    {
        cmds[11] = 0x12;
    }

    for (auto value : cmds)
    {
        sendCmd(value);
    }
}

void ssd1306os::sendCmd(uint8_t cmdP)
{
    // I2C write process expects a control byte followed by data
    // this "data" can be a command or data to follow up a command
    // Co = 1, D/C = 0 => the driver expects a command
    uint8_t buf[2]{0x80, cmdP};
    pSsd1306I2cM->write(addressM, buf, 2);
}

void ssd1306os::show()
{
    uint16_t x0 = 0;
    uint16_t x1 = widthM - 1;
    if (widthM != 128)
    {
        // narrow displays use centred columns
        uint16_t colOffset = (128 - widthM); // 2
        x0 += colOffset;
        x1 += colOffset;
    }
    sendCmd(SSD1306_SET_COL_ADDR);
    sendCmd(x0);
    sendCmd(x1);
    sendCmd(SSD1306_SET_PAGE_ADDR);
    sendCmd(0);
    sendCmd(heightM / 8 - 1); // nr of pages??
    // set control byte at the beginning of frame buffer
    bufferM.get()[0] = 0x40;
    // write the frame buffer at one go
    pSsd1306I2cM->write(addressM, bufferM.get(), sizeM);
}
