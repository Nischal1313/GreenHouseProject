#pragma once

#include "queue.h"
#include <memory>

struct DebugEvent
{
    char msg[128];
    uint32_t timestamp;
};

class Debug
{
public:
    Debug();

    void print(char const *pTxtP, ...) const;

    [[nodiscard]] DebugEvent getEvent() const;

private:
    QueueHandle_t m_queue;
};

class DebugTask
{
public:
    explicit DebugTask(std::shared_ptr<Debug> pDebugP);

    [[noreturn]] void run() const;

private:
    std::shared_ptr<Debug> m_debug;
};

void debug(char const *pTxtP, ...);
