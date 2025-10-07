
// --- inputManager.h - Updated header ---
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

    // Get button press events (edge-triggered, consumed on read)
    bool getMenuPressEvent();
    bool getNextFieldPressEvent();
    bool getCharsetPressEvent();

    // Getters for continuous button states
    bool isMenuPressed() const;
    bool isNextFieldPressed() const;
    bool isCharsetPressed() const;

private:
    // Continuous button states
    volatile bool menuPressed;
    volatile bool nextFieldPressed;
    volatile bool charsetPressed;

    // Button press events (edge-triggered)
    volatile bool menuPressEvent;
    volatile bool nextFieldPressEvent;
    volatile bool charsetPressEvent;

    // Internal helpers
    static bool readButton(uint gpio);

    // GPIO pin numbers (fixed)
    static constexpr uint MENU_PIN = 7;
    static constexpr uint NEXT_FIELD_PIN = 8;
    static constexpr uint CHARSET_PIN = 9;
};

#endif