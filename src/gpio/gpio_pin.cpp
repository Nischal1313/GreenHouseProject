#include "gpio_pin.h"
#include <cstdio>

GPIOPin::GPIOPin(
    uint pinP,
    GPIOMode modeP,
    GPIOPull pullP,
    bool invertP,
    uint32_t debounceMsP) :
    pinNumberM{pinP},
    modeM{modeP},
    pullM{pullP},
    isInvertedM{invertP},
    lastReadingM{false},
    stableStateM{false},
    pressEventM{false},
    holdEventM{false},
    debounceMsM{debounceMsP},
    holdMsM{1000}
{
    gpio_init(pinP);
    gpio_set_dir(pinP, modeP == GPIOMode::OUTPUT ? GPIO_OUT : GPIO_IN);

    if (pullP == GPIOPull::PULLUP)
    {
        gpio_pull_up(pinP);
    }
    else if (pullP == GPIOPull::PULLDOWN)
    {
        gpio_pull_down(pinP);
    }

    if (invertP)
    {
        if (modeP == GPIOMode::INPUT)
        {
            gpio_set_inover(pinP, GPIO_OVERRIDE_INVERT);
        }
        else
        {
            gpio_set_outover(pinP, GPIO_OVERRIDE_INVERT);
        }
    }

    lastChangeTimeM = get_absolute_time();
    pressStartTimeM = get_absolute_time();
}

bool GPIOPin::read() const
{
    bool result;

    if (modeM != GPIOMode::INPUT)
    {
        printf("[GPIO WARNING] Attempted to read from OUTPUT pin %d\n", pinNumberM);
        result = false;
    }
    else
    {
        bool val = gpio_get(pinNumberM);
        result = isInvertedM ? !val : val;
    }

    return result;
}

void GPIOPin::write(bool valueP) const
{
    if (modeM != GPIOMode::OUTPUT)
    {
        printf("[GPIO WARNING] Attempted to write to INPUT pin %d\n", pinNumberM);
    }
    else
    {
        gpio_put(pinNumberM, isInvertedM ? !valueP : valueP);
    }
}

int GPIOPin::getPin() const
{
    return pinNumberM;
}

void GPIOPin::update()
{
    if (modeM == GPIOMode::INPUT)
    {
        bool reading = !gpio_get(pinNumberM);
        absolute_time_t now = get_absolute_time();

        if (reading != lastReadingM)
        {
            lastChangeTimeM = now;
        }

        if (absolute_time_diff_us(lastChangeTimeM, now)
            > static_cast<int64_t>(debounceMsM) * 1000)
        {
            if (reading != stableStateM)
            {
                stableStateM = reading;

                if (stableStateM)
                {
                    pressEventM = true;
                    pressStartTimeM = now;
                    holdEventM = false;
                }
                else
                {
                    holdEventM = false;
                }
            }
        }

        if (stableStateM
            && !holdEventM
            && absolute_time_diff_us(pressStartTimeM, now)
                > static_cast<int64_t>(holdMsM) * 1000)
        {
            holdEventM = true;
        }

        lastReadingM = reading;
    }
}

bool GPIOPin::pressed()
{
    bool result = false;

    if (pressEventM)
    {
        pressEventM = false;
        result = true;
    }

    return result;
}

bool GPIOPin::held()
{
    bool result = false;

    if (holdEventM)
    {
        holdEventM = false;
        result = true;
    }

    return result;
}

void GPIOPin::setHoldTime(uint32_t msP)
{
    holdMsM = msP;
}

void GPIOPin::setDebounceTime(uint32_t msP)
{
    debounceMsM = msP;
}
