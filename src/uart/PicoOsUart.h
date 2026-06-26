//
// Created by Keijo Länsikunnas on 30.8.2024.
//

#pragma once

#include <hardware/uart.h>
#include <hardware/irq.h>
#include <string>
#include "FreeRTOS.h"
#include "queue.h"
#include "Fmutex.h"

class PicoOsUart
{
    friend void picoUart0Handler();
    friend void picoUart1Handler();

public:
    PicoOsUart(
        int uartNrP,
        int txPinP,
        int rxPinP,
        int speedP,
        int stopP = 1,
        int txSizeP = 256,
        int rxSizeP = 256);

    PicoOsUart(PicoOsUart const &) = delete;

    int read(uint8_t *pBufferP, int sizeP, TickType_t timeoutP = pdMS_TO_TICKS(500));
    int write(uint8_t const *pBufferP, int sizeP, TickType_t timeoutP = pdMS_TO_TICKS(500));
    int send(char const *strP);
    int send(std::string const &rStrP);
    int flush();
    int getFifoLevel();
    int getBaud() const;

private:
    void uartIrqRx();
    void uartIrqTx();

    Fmutex accessM;
    QueueHandle_t txM;
    QueueHandle_t rxM;
    uart_inst_t *uartM;
    int irqnM;
    int speedM;
};
