#pragma once

#include <FreeRTOS.h>
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"

class MutexGuard
{
public:
    explicit MutexGuard(SemaphoreHandle_t mutexP) :
        mutexM{mutexP},
        lockedM{false}
    {
        if (mutexM && xSemaphoreTake(mutexM, portMAX_DELAY) == pdTRUE)
        {
            lockedM = true;
        }
    }

    ~MutexGuard()
    {
        if (lockedM)
        {
            xSemaphoreGive(mutexM);
        }
    }

    [[nodiscard]] bool owns_lock() const
    {
        return lockedM;
    }

    MutexGuard(MutexGuard const &) = delete;
    MutexGuard &operator=(MutexGuard const &) = delete;

    MutexGuard(MutexGuard &&otherP) noexcept :
        mutexM{otherP.mutexM},
        lockedM{otherP.lockedM}
    {
        otherP.lockedM = false;
    }

    MutexGuard &operator=(MutexGuard &&otherP) noexcept
    {
        if (this != &otherP)
        {
            if (lockedM)
            {
                xSemaphoreGive(mutexM);
            }

            mutexM = otherP.mutexM;
            lockedM = otherP.lockedM;
            otherP.lockedM = false;
        }

        return *this;
    }

private:
    SemaphoreHandle_t mutexM;
    bool lockedM;
};
