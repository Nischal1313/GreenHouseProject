//
// Created by Keijo Länsikunnas on 10.9.2024.
//

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "hardware/i2c.h"
#include "Fmutex.h"

class PicoI2C
{
public:
    explicit PicoI2C(uint busNrP, uint speedP = 100000);
    PicoI2C(PicoI2C const &) = delete;
    uint write(uint8_t addrP, uint8_t const *pBufferP, uint lengthP);
    uint read(uint8_t addrP, uint8_t *pBufferP, uint lengthP);
    uint transaction(uint8_t addrP, uint8_t const *pWbufferP, uint wlengthP, uint8_t *pRbufferP, uint rlengthP);

private:
    i2c_inst *i2cM;
    int irqnM;
    TaskHandle_t taskToNotifyM;
    Fmutex accessM;
    uint8_t const *wbufM;
    uint wctrM;
    uint8_t *rbufM;
    uint rctrM;
    uint rcntM;

    void txFillFifo();
    void rxFillFifo();
    void isr();

    static void i2c0Irq();
    static void i2c1Irq();
    static PicoI2C *i2c0InstanceS;
    static PicoI2C *i2c1InstanceS;
};
