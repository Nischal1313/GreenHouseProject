//
// Created by Keijo Länsikunnas on 10.9.2024.
//

#include <mutex>
#include "pico/stdlib.h"
#include "PicoI2C.h"

constexpr bool DEBUG_PRINT = false;

#if DEBUG_PRINT
#include "Syslog.h"
#endif

constexpr uint I2C0_SDA_PIN = 16;
constexpr uint I2C0_SCL_PIN = 17;
constexpr uint I2C1_SDA_PIN = 14;
constexpr uint I2C1_SCL_PIN = 15;
constexpr uint RX_TL_VALUE = 14;

PicoI2C *PicoI2C::i2c0InstanceS{nullptr};
PicoI2C *PicoI2C::i2c1InstanceS{nullptr};

void PicoI2C::i2c0Irq()
{
    if (i2c0InstanceS)
    {
        i2c0InstanceS->isr();
    }
    else
    {
        irq_set_enabled(I2C0_IRQ, false);
    }
}

void PicoI2C::i2c1Irq()
{
    if (i2c1InstanceS)
    {
        i2c1InstanceS->isr();
    }
    else
    {
        irq_set_enabled(I2C1_IRQ, false);
    }
}

PicoI2C::PicoI2C(uint busNrP, uint speedP) :
    taskToNotifyM{nullptr},
    wbufM{nullptr},
    wctrM{0},
    rbufM{nullptr},
    rctrM{0},
    rcntM{0}
{
    int scl = I2C0_SCL_PIN;
    int sda = I2C0_SDA_PIN;

    switch (busNrP)
    {
        case 0:
            i2cM = i2c0;
            irqnM = I2C0_IRQ;
            break;

        case 1:
            i2cM = i2c1;
            irqnM = I2C1_IRQ;
            scl = I2C1_SCL_PIN;
            sda = I2C1_SDA_PIN;
            break;

        default:
            panic("Invalid I2C bus number\n");
            break;
    }

    gpio_init(scl);
    gpio_pull_up(scl);
    gpio_init(sda);
    gpio_pull_up(sda);
    irq_set_enabled(irqnM, false);
    irq_set_exclusive_handler(irqnM, busNrP ? i2c1Irq : i2c0Irq);
    i2c_init(i2cM, speedP);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    i2cM->hw->tx_tl = 0;
    i2cM->hw->rx_tl = RX_TL_VALUE;

    if (busNrP)
    {
        i2c1InstanceS = this;
    }
    else
    {
        i2c0InstanceS = this;
    }
}

void PicoI2C::txFillFifo()
{
#if DEBUG_PRINT
    int fill{0};
#endif

    while (wctrM > 0 && i2c_get_write_available(i2cM) > 0)
    {
        bool last = wctrM == 1;
        bool stop = rctrM == 0;
        i2cM->hw->data_cmd =
            bool_to_bit(i2cM->restart_on_next) << I2C_IC_DATA_CMD_RESTART_LSB
            | bool_to_bit(last && stop) << I2C_IC_DATA_CMD_STOP_LSB
            | *wbufM++;

        if (i2cM->restart_on_next)
        {
            i2cM->restart_on_next = false;
        }

        --wctrM;

        if (last && !stop)
        {
            i2cM->restart_on_next = true;
        }

#if DEBUG_PRINT
        ++fill;
#endif
    }

#if DEBUG_PRINT
    Syslog::debug("tx_fill: %d", fill);
#endif
}

void PicoI2C::rxFillFifo()
{
#if DEBUG_PRINT
    int fill{0};
#endif

    while (rctrM > 0 && i2c_get_write_available(i2cM) > 0)
    {
        bool last = rctrM == 1;
        i2cM->hw->data_cmd =
            bool_to_bit(i2cM->restart_on_next) << I2C_IC_DATA_CMD_RESTART_LSB
            | bool_to_bit(last) << I2C_IC_DATA_CMD_STOP_LSB
            | I2C_IC_DATA_CMD_CMD_BITS;

        if (i2cM->restart_on_next)
        {
            i2cM->restart_on_next = false;
        }

        --rctrM;

#if DEBUG_PRINT
        ++fill;
#endif
    }

#if DEBUG_PRINT
    Syslog::debug("rx_fill: %d", fill);
#endif
}

uint PicoI2C::write(uint8_t addrP, uint8_t const *pBufferP, uint lengthP)
{
    uint result = transaction(addrP, pBufferP, lengthP, nullptr, 0);

    return result;
}

uint PicoI2C::read(uint8_t addrP, uint8_t *pBufferP, uint lengthP)
{
    uint result = transaction(addrP, nullptr, 0, pBufferP, lengthP);

    return result;
}

uint PicoI2C::transaction(
    uint8_t addrP,
    uint8_t const *pWbufferP,
    uint wlengthP,
    uint8_t *pRbufferP,
    uint rlengthP)
{
    assert((pWbufferP && wlengthP > 0) || (pRbufferP && rlengthP > 0));

    uint result;

    {
        std::lock_guard<Fmutex> exclusive(accessM);
        taskToNotifyM = xTaskGetCurrentTaskHandle();

        i2cM->hw->enable = 0;
        i2cM->hw->tar = addrP;
        i2cM->hw->enable = 1;
        i2cM->hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS
            | I2C_IC_INTR_MASK_M_TX_EMPTY_BITS
            | I2C_IC_INTR_MASK_M_RX_FULL_BITS;
        i2cM->restart_on_next = false;

        wbufM = pWbufferP;
        wctrM = wlengthP;
        rbufM = pRbufferP;
        rctrM = rlengthP;
        rcntM = rlengthP;

        if (wctrM > 0)
        {
            txFillFifo();
        }
        else
        {
            rxFillFifo();
        }

        irq_set_enabled(irqnM, true);

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0)
        {
            result = 0;
        }
        else
        {
            result = wlengthP + rlengthP - rcntM - wctrM;
        }

        irq_set_enabled(irqnM, false);
    }

    return result;
}

void PicoI2C::isr()
{
    BaseType_t hpw = pdFALSE;

#if DEBUG_PRINT
    Syslog::debug("%d %d %d %d",
        !!(i2cM->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS),
        !!(i2cM->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_RX_FULL_BITS),
        !!(i2cM->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS),
        !!(i2cM->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_RX_OVER_BITS));
#endif

#if DEBUG_PRINT
    int fill{0};
#endif

    while (rcntM > 0 && i2cM->hw->rxflr > 0)
    {
        *rbufM++ = static_cast<uint8_t>(i2cM->hw->data_cmd);
        --rcntM;

#if DEBUG_PRINT
        ++fill;
#endif
    }

#if DEBUG_PRINT
    Syslog::debug("read: %d, %d", fill, rcntM);
#endif

    if (i2cM->hw->intr_stat & I2C_IC_INTR_MASK_M_TX_EMPTY_BITS)
    {
        if (wctrM > 0)
        {
            txFillFifo();
        }
        else if (rctrM > 0)
        {
            rxFillFifo();
        }

        if (wctrM == 0 && rctrM == 0)
        {
            i2cM->hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS
                | I2C_IC_INTR_MASK_M_RX_FULL_BITS;
        }
    }

    if (i2cM->hw->intr_stat & I2C_IC_INTR_MASK_M_STOP_DET_BITS)
    {
        i2cM->hw->intr_mask = 0;
        (void)i2cM->hw->clr_stop_det;
        xTaskNotifyFromISR(taskToNotifyM, 1, eSetValueWithOverwrite, &hpw);
    }

    portYIELD_FROM_ISR(hpw);
}
