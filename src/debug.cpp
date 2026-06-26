#include <cstdarg>
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "debug.h"

#include <memory>
#include <utility>

namespace
{
    void debugTask(void *pvParametersP)
    {
        auto const pTask = static_cast<DebugTask *>(pvParametersP);
        pTask->run();
    }
}

Debug::Debug() : m_queue{xQueueCreate(20, sizeof(DebugEvent))}
{
}

void Debug::print(char const *pTxtP, ...) const
{
    DebugEvent e{};
    e.timestamp = xTaskGetTickCount();

    va_list args;
    va_start(args, pTxtP);
    vsnprintf(e.msg, sizeof(e.msg), pTxtP, args);
    va_end(args);

    // Non-blocking send, events can be lost
    xQueueSend(m_queue, &e, 0);
}

DebugEvent Debug::getEvent() const
{
    DebugEvent e{};
    xQueueReceive(m_queue, &e, portMAX_DELAY);
    return e;
}

DebugTask::DebugTask(std::shared_ptr<Debug> pDebugP) : m_debug{
    std::move(pDebugP)
}
{
    constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;

    xTaskCreate(debugTask, "DebugTask", 1024, this, TASK_LOW_PRIORITY,
                nullptr);
}

[[noreturn]] void DebugTask::run() const
{
    while (true) {
        auto [msg, timestamp] = m_debug->getEvent();
        printf("[%lu] %s", static_cast<unsigned long>(timestamp), msg);
    }
}
