//
// Created by Keijo Länsikunnas on 30.8.2024.
//

#include "PicoOsUart.h"
#include <mutex>
#include <hardware/gpio.h>
#include <cstring>

namespace
{
    PicoOsUart *pu0S;
    PicoOsUart *pu1S;
}

void picoUart0Handler()
{
    if (pu0S)
    {
        pu0S->uartIrqRx();
        pu0S->uartIrqTx();
    }
    else
    {
        irq_set_enabled(UART0_IRQ, false);
    }
}

void picoUart1Handler()
{
    if (pu1S)
    {
        pu1S->uartIrqRx();
        pu1S->uartIrqTx();
    }
    else
    {
        irq_set_enabled(UART1_IRQ, false);
    }
}

PicoOsUart::PicoOsUart(
    int uartNrP,
    int txPinP,
    int rxPinP,
    int speedP,
    int stopP,
    int txSizeP,
    int rxSizeP) :
    speedM{speedP}
{
    txM = xQueueCreate(txSizeP, sizeof(char));
    rxM = xQueueCreate(rxSizeP, sizeof(char));
    irqnM = uartNrP == 0 ? UART0_IRQ : UART1_IRQ;
    uartM = uartNrP == 0 ? uart0 : uart1;

    if (uartNrP == 0)
    {
        pu0S = this;
    }
    else
    {
        pu1S = this;
    }

    irq_set_enabled(irqnM, false);
    uart_init(uartM, speedP);
    uart_set_format(uartM, 8, stopP, UART_PARITY_NONE);
    gpio_set_function(txPinP, GPIO_FUNC_UART);
    gpio_set_function(rxPinP, GPIO_FUNC_UART);
    irq_set_exclusive_handler(irqnM, uartNrP == 0 ? picoUart0Handler : picoUart1Handler);
    uart_set_irq_enables(uartM, true, false);
    irq_set_enabled(irqnM, true);
}

int PicoOsUart::read(uint8_t *pBufferP, int sizeP, TickType_t timeoutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);

    int result = 0;

    while (result < sizeP && xQueueReceive(rxM, pBufferP, timeoutP) == pdTRUE)
    {
        ++pBufferP;
        ++result;
    }

    return result;
}

int PicoOsUart::write(uint8_t const *pBufferP, int sizeP, TickType_t timeoutP)
{
    std::lock_guard<Fmutex> exclusive(accessM);

    int result = 0;

    while (result < sizeP && xQueueSendToBack(txM, pBufferP, timeoutP) == pdTRUE)
    {
        ++pBufferP;
        ++result;
    }

    irq_set_enabled(irqnM, false);

    if (!(uart_get_hw(uartM)->imsc & (1 << UART_UARTIMSC_TXIM_LSB)))
    {
        uint8_t ch;

        while (uart_is_writable(uartM) && xQueueReceive(txM, &ch, 0) == pdTRUE)
        {
            uart_get_hw(uartM)->dr = ch;
        }

        if (uxQueueMessagesWaiting(txM) > 0)
        {
            uart_set_irq_enables(uartM, true, true);
        }
    }

    irq_set_enabled(irqnM, true);

    return result;
}

int PicoOsUart::send(char const *strP)
{
    write(
        reinterpret_cast<uint8_t const *>(strP),
        static_cast<int>(strlen(strP)));

    int result = 0;

    return result;
}

int PicoOsUart::send(std::string const &rStrP)
{
    write(
        reinterpret_cast<uint8_t const *>(rStrP.c_str()),
        static_cast<int>(rStrP.length()));

    int result = 0;

    return result;
}

int PicoOsUart::flush()
{
    std::lock_guard<Fmutex> exclusive(accessM);

    int result = 0;
    char dummy = 0;

    while (xQueueReceive(rxM, &dummy, 0) == pdTRUE)
    {
        ++result;
    }

    return result;
}

void PicoOsUart::uartIrqRx()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (uart_is_readable(uartM))
    {
        uint8_t c = uart_getc(uartM);
        xQueueSendToBackFromISR(rxM, &c, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void PicoOsUart::uartIrqTx()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ch;

    while (uart_is_writable(uartM)
        && xQueueReceiveFromISR(txM, &ch, &xHigherPriorityTaskWoken) == pdTRUE)
    {
        uart_get_hw(uartM)->dr = ch;
    }

    if (xQueueIsQueueEmptyFromISR(txM))
    {
        uart_set_irq_enables(uartM, true, false);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int PicoOsUart::getFifoLevel()
{
    uint8_t const flv[]{4, 8, 16, 24, 28, 0, 0, 0, 0};
    uint32_t lcr_h = uart_get_hw(uartM)->lcr_h;
    uint32_t fcr = (uart_get_hw(uartM)->ifls >> 3) & 0x7;

    if (!(lcr_h | UART_UARTLCR_H_FEN_BITS))
    {
        fcr = 8;
    }

    return flv[fcr];
}

int PicoOsUart::getBaud() const
{
    return speedM;
}
