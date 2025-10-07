#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hardware/gpio.h"

class InputManager {
public:
    InputManager();

    // Start FreeRTOS task
    static void taskEntry(void *pvParameters);

    [[noreturn]] void inputTask();

    // Getters for button states
    bool isMenuPressed() const;

    bool isNextFieldPressed() const;

    bool isCharsetPressed() const;

private:
    // Button states
    volatile bool menuPressed ;
    volatile bool nextFieldPressed ;
    volatile bool charsetPressed;

    // Internal helpers
    static bool readButton(uint gpio);

    void handleButton(uint gpio, volatile bool &flag);

    // GPIO pin numbers (fixed)
    static constexpr uint MENU_PIN = 7;
    static constexpr uint NEXT_FIELD_PIN = 8;
    static constexpr uint CHARSET_PIN = 9;
    static constexpr uint32_t DEBOUNCE_MS = 150; // 150 ms debounce
};

#endif
