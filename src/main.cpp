#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include "FreeRTOS.h"
#include "event_groups.h"
#include "pico/stdlib.h"
#include "task.h"
#include "timers.h"
#include "hardware/timer.h"
#include "uart/PicoOsUart.h"
#include "modbus/ModbusClient.h"
#include "modbus/ModbusRegister.h"
#include "semphr.h"
#include "produalMIO.h"
#include "mutexGuard.h"

extern "C" {
    uint32_t read_runtime_ctr(void) {
        return time_us_32();
    }
}

constexpr int TASK_HIGH_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int WATCHDOG_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;

constexpr int BUTTON1_PIN = 7;
constexpr int BUTTON2_PIN = 8;
constexpr int BUTTON3_PIN = 9;

// Event group bits
constexpr EventBits_t BIT_TASK1 = (1 << 0);
constexpr EventBits_t BIT_TASK2 = (1 << 1);
constexpr EventBits_t BIT_TASK3 = (1 << 2);


// // Uart init
auto uart = std::make_shared<PicoOsUart>(0, 0, 1, 9600, 1, 256, 256);

// Modbus client
auto modbus = std::make_shared<ModbusClient>(uart);

// Mutex handler
SemaphoreHandle_t modbusMutex;

// Correctly instantiate the ModbusMIO object.
ModbusMIO modbusFan(modbus, 1,modbusMutex );

// Queue handle for debug events
QueueHandle_t syslog_q;
constexpr int DEBUG_QUEUE_LENGTH = 10;

// Event group handle
EventGroupHandle_t eventGroup;

// 30s watchdog timer
constexpr TickType_t WATCHDOG_TIMER = pdMS_TO_TICKS(30000);

// Create a mutex to protect button access
SemaphoreHandle_t buttonMutex;

// Debug event structure
struct debugEvent {
    char debug[64]; // message
    uint32_t timestamp; // tick count when event created
};

// Parameter structure for tasks
struct TaskParams {
    int buttonPin;
    EventBits_t eventBit;
};

void initFunction() {

    // Setup buttons
    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);

    gpio_init(BUTTON3_PIN);
    gpio_set_dir(BUTTON3_PIN, GPIO_IN);
    gpio_pull_up(BUTTON3_PIN);

}

void debug(const char *message) {
    debugEvent e{};
    e.timestamp = xTaskGetTickCount();
    snprintf(e.debug, sizeof(e.debug), "%s", message);
    xQueueSend(syslog_q, &e, pdMS_TO_TICKS(10));
}

[[noreturn]] void debugTask(void *pvParameters) {
    debugEvent e{};
    while (true) {
        // Wait indefinitely for a debug event
        if (xQueueReceive(syslog_q, &e, portMAX_DELAY) == pdPASS) {
            std::cout << "[" << e.timestamp << "] " << e.debug;
        }
    }
}

// Watchdog task (Task 4)
[[noreturn]] void watchDogTimer(void *pvParameter) {
    constexpr EventBits_t ALL_BITS = BIT_TASK1 | BIT_TASK2 | BIT_TASK3;
    TickType_t lastOK = xTaskGetTickCount();

    while (true) {
        // Wait up to 30s for all three tasks to report
        EventBits_t result = xEventGroupWaitBits(
            eventGroup,
            ALL_BITS,
            pdTRUE, // clear bits on exit
            pdTRUE, // wait for all bits
            WATCHDOG_TIMER
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
            // Timeout -> check missing tasks
            EventBits_t missingBits = ALL_BITS & ~result;
            debug("Watchdog FAIL! Missing tasks:\n");
            if (missingBits & BIT_TASK1) debug("Task One\n");
            if (missingBits & BIT_TASK2) debug("Task Two\n");
            if (missingBits & BIT_TASK3) debug("Task Three\n");
            vTaskSuspend(nullptr); // suspend watchdog
        }
    }
}

// A new task to demonstrate Modbus functionality
void modbusTask(void *pvParameters) {
    constexpr float desiredFanSpeed = 100.0f;

    // FIX: Set the correct slave address for the Modbus device
    modbus->set_destination_rtu_address(1);

    if (modbusFan.setFanSpeed(desiredFanSpeed)) {
        debug("Fan speed set successfully\n");
    } else {
        debug("Failed to set fan speed\n");
    }
    vTaskSuspend(nullptr); // Suspend this task after it runs
}

int main() {
    stdio_init_all();
    initFunction();

    // Create event group and debug queue
    eventGroup = xEventGroupCreate();
    syslog_q = xQueueCreate(DEBUG_QUEUE_LENGTH, sizeof(debugEvent));
    buttonMutex = xSemaphoreCreateMutex();
    printf("Program started.\n");

    xTaskCreate(debugTask, "DebugTask", 256,
                nullptr,
                TASK_LOW_PRIORITY, nullptr);

    xTaskCreate(watchDogTimer, "WatchDogTimer", 256,
                nullptr,
                WATCHDOG_PRIORITY, nullptr);

    xTaskCreate(modbusTask, "ModbusTask", 256,
                nullptr,
                TASK_HIGH_PRIORITY, nullptr);

    vTaskStartScheduler();
    while (true); // should never reach
}

