#ifndef DEBUG_H
#define DEBUG_H

#include "queue.h"
#include <memory>

struct DebugEvent {
    char msg[128];
    uint32_t timestamp;
};

class Debug {
public:
    Debug();
    void print(const char *txt, ...);
    DebugEvent getEvent();
private:
    QueueHandle_t m_queue;
};


class DebugTask {
public:
    explicit DebugTask(std::shared_ptr<Debug> debug);
    [[noreturn]] void run() const;
private:
    std::shared_ptr<Debug> m_debug;
};


// void debugInit();
void debug(const char *txt, ...);
void createQueue();

//extern QueueHandle_t debugQueue;

#endif // DEBUG_H
