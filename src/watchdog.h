#pragma once

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"
#include <cstdint>

/**
 * @brief FreeRTOS Watchdog implementation using Event Groups.
 *
 * This class monitors up to 6 tasks. Each task must periodically
 * set its bit in the event group. If all tasks report in within
 * the timeout, the watchdog prints "OK". Otherwise, it reports
 * missing tasks and suspends itself.
 */
class Watchdog
{
public:
    /**
     * @brief Construct a Watchdog instance.
     * @param groupP The event group handle (must be created beforehand).
     * @param timeoutTicksP Timeout in FreeRTOS ticks before declaring failure.
     */
    Watchdog(EventGroupHandle_t groupP, TickType_t timeoutTicksP);

    /**
     * @brief Start the watchdog task.
     */
    void start();

    /**
     * @brief Get the event bit for a given task index (1–6).
     * @param taskIndexP Task number (1–6).
     * @return Event bit mask for that task.
     */
    static EventBits_t taskBit(int taskIndexP);

private:
    EventGroupHandle_t eventGroupM;
    TickType_t timeoutTicksM;
    static int constexpr MAX_TASKS{6};

    // Bitmask of all monitored tasks
    static EventBits_t constexpr ALL_BITS{
        (1 << 0) | (1 << 1) | (1 << 2) |
        (1 << 3) | (1 << 4) | (1 << 5)
    };

    /**
     * @brief The watchdog task loop.
     * @param pParamP Pointer to Watchdog instance.
     */
    static void taskLoop(void *pParamP);

    /**
     * @brief Helper to print debug output.
     */
    static void debug(char const *pMsgP);
};
