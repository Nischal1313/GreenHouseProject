#pragma once
#include <memory>
#include "ssd1306os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "hardware/gpio.h"
#include <cstdint>

class HANDLEC02INPUT {
public:
    // Hardcoded pins (no magic numbers in main)
    static constexpr uint8_t ENCODER_PIN_A = 10;
    static constexpr uint8_t ENCODER_PIN_B = 11;
    static constexpr uint8_t BUTTON_PIN    = 8;

    // Constructor auto-initializes pins, semaphore, queue and starts no tasks by itself.
    HANDLEC02INPUT();

    // Start the internal FreeRTOS tasks (encoder handler + display sender)
    // Pass the shared oled pointer (by const ref) so we don't copy unnecessarily.
    void startTasks(const std::shared_ptr<ssd1306os>& oled);

    // Getter for the setpoint. -1 == unset.
    [[nodiscard]] int getDesiredValue() const;

    // Destructor cleans up (not strictly necessary on embedded where app runs forever).
    ~HANDLEC02INPUT();

private:
    // Internal state
    volatile int desiredCO2Value; // -1 == unset
    volatile int lockedValue;     // last locked value (or -1)
    volatile bool locked;         // currently locked for 40s
    volatile TickType_t lockTick; // tick when locked
    volatile TickType_t lastChangeTick; // last rotation tick (for 3s revert)

    // RTOS objects owned by instance
    SemaphoreHandle_t gpioSem; // given from ISR
    QueueHandle_t displayQueue; // messages for display
    TaskHandle_t encoderTaskHandle;
    TaskHandle_t displayWorkerHandle;

    // private methods
    static void gpio_isr(uint gpio, uint32_t events); // ISR forwarding to semaphore
    static void encoderTaskFn(void* pv);
    static void displayWorkerFn(void* pv);

    // small struct for queue messages
    struct DisplayMsg {
        char text[40];
        uint8_t y; // vertical position on screen
    };
};
