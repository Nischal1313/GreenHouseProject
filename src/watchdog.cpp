#include "watchdog.h"
#include <cstdio>

Watchdog::Watchdog(EventGroupHandle_t groupP, TickType_t timeoutTicksP)
  : eventGroupM{groupP}, timeoutTicksM{timeoutTicksP}
{
}

void Watchdog::start()
{
    xTaskCreate(taskLoop, "WatchdogTask", 256, this, 1, nullptr);
}

EventBits_t Watchdog::taskBit(int taskIndexP)
{
    EventBits_t result{0};
    if (taskIndexP >= 1 && taskIndexP <= MAX_TASKS) {
        result = EventBits_t{1} << (taskIndexP - 1);
    }
    return result;
}

void Watchdog::taskLoop(void *pParamP)
{
    auto *pSelf = static_cast<Watchdog *>(pParamP);
    TickType_t lastOK = xTaskGetTickCount();

    while (true) {
        EventBits_t result = xEventGroupWaitBits(
            pSelf->eventGroupM,
            ALL_BITS,
            pdTRUE, // clear bits on exit
            pdTRUE, // wait for all bits
            pSelf->timeoutTicksM
        );

        if ((result & ALL_BITS) == ALL_BITS) {
            TickType_t now = xTaskGetTickCount();
            char buf[64];
            snprintf(buf, sizeof(buf),
                     "Watchdog: OK, %lu ticks since last OK\n",
                     static_cast<unsigned long>(now - lastOK));
            debug(buf);
            lastOK = now;
        } else {
            EventBits_t missingBits = ALL_BITS & ~result;
            debug("Watchdog FAIL! Missing tasks:\n");

            for (int i = 1; i <= MAX_TASKS; i++) {
                if (missingBits & taskBit(i)) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Task %d\n", i);
                    debug(buf);
                }
            }

            vTaskSuspend(nullptr); // suspend watchdog
        }
    }
}

void Watchdog::debug(char const *pMsgP)
{
    printf("%s", pMsgP);
}
