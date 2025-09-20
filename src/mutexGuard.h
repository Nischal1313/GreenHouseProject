#pragma once
#include <FreeRTOS.h>
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"

/**
 * @brief RAII wrapper for FreeRTOS mutexes.
 *
 * Acquires the given mutex in the constructor, releases it in the destructor.
 * Ensures safe and exception-proof locking for shared resources.
 */
class MutexGuard {
public:
    explicit MutexGuard(const SemaphoreHandle_t m)
        : mutex(m), locked(false) {
        if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            locked = true;
        }
    }

    ~MutexGuard() {
        if (locked) {
            xSemaphoreGive(mutex);
        }
    }

    [[nodiscard]] bool owns_lock() const { return locked; }

    // Non-copyable
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

private:
    SemaphoreHandle_t mutex;
    bool locked;
};
