#include "watchdog.h"
#include <cstdio>
#include <cstring>

// =====================
// Constructor
// =====================
Watchdog::Watchdog(EventGroupHandle_t group, TickType_t timeoutTicks)
    : eventGroup(group), timeoutTicks(timeoutTicks) {}

// =====================
// Public API
// =====================
void Watchdog::start() {
    xTaskCreate(taskLoop, "WatchdogTask", 256, this, 1, nullptr);
}

EventBits_t Watchdog::taskBit(int taskIndex) {
    if (taskIndex < 1 || taskIndex > MAX_TASKS) return 0;
    return (1 << (taskIndex - 1));
}

// =====================
// Task Loop
// =====================
void Watchdog::taskLoop(void *param) {
    auto *self = static_cast<Watchdog *>(param);
    TickType_t lastOK = xTaskGetTickCount();

    while (true) {
        EventBits_t result = xEventGroupWaitBits(
            self->eventGroup,
            ALL_BITS,
            pdTRUE,     // clear bits on exit
            pdTRUE,     // wait for all bits
            self->timeoutTicks
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

// =====================
// Debug helper
// =====================
void Watchdog::debug(const char *msg) {
    // Replace this with your system's debug queue or logging
    printf("%s", msg);
}
