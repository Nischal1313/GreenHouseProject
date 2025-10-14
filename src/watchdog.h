#ifndef WATCHDOG_H
#define WATCHDOG_H

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
class Watchdog {
public:
  /**
   * @brief Construct a Watchdog instance.
   * @param group The event group handle (must be created beforehand).
   * @param timeoutTicks Timeout in FreeRTOS ticks before declaring failure.
   */
  Watchdog(EventGroupHandle_t group, TickType_t timeoutTicks);

  /**
   * @brief Start the watchdog task.
   */
  void start();

  /**
   * @brief Get the event bit for a given task index (1–6).
   * @param taskIndex Task number (1–6).
   * @return Event bit mask for that task.
   */
  static EventBits_t taskBit(int taskIndex);

private:
  EventGroupHandle_t eventGroup;
  TickType_t timeoutTicks;
  static constexpr int MAX_TASKS = 6;

  // Bitmask of all monitored tasks
  static constexpr EventBits_t ALL_BITS =
      (1 << 0) | (1 << 1) | (1 << 2) |
      (1 << 3) | (1 << 4) | (1 << 5);

  /**
   * @brief The watchdog task loop.
   * @param param Pointer to Watchdog instance.
   */
  static void taskLoop(void *param);

  /**
   * @brief Helper to print debug output.
   */
  static void debug(const char *msg);
};

#endif // WATCHDOG_H
