#ifndef MUTEX_GUARD
#define MUTEX_GUARD

#include <FreeRTOS.h>
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"

/**
 * @brief RAII wrapper for FreeRTOS mutexes.
 *
 * Acquires the given mutex in the constructor, releases it in the destructor.
 * Ensures safe and exception-proof locking for shared resources.
 *
 * - Non-copyable (cannot be copied, avoids double release).
 * - Movable (can be returned from functions or stored in containers).
 */
class MutexGuard {
public:
    explicit MutexGuard(SemaphoreHandle_t m)
        : mutex(m), locked(false) {
        if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            locked = true;
        }
    }

    // Destructor releases the mutex if locked
    ~MutexGuard() {
        if (locked) {
            xSemaphoreGive(mutex);
        }
    }

    [[nodiscard]] bool owns_lock() const { return locked; }

    // Non-copyable
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    // Movable
    MutexGuard(MutexGuard&& other) noexcept
        : mutex(other.mutex), locked(other.locked) {
        other.locked = false;
    }

    MutexGuard& operator=(MutexGuard&& other) noexcept {
        if (this != &other) {
            if (locked) {
                xSemaphoreGive(mutex);
            }
            mutex = other.mutex;
            locked = other.locked;
            other.locked = false;
        }
        return *this;
    }

private:
    SemaphoreHandle_t mutex;
    bool locked;
};

#endif
