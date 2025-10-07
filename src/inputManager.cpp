#include "inputManager.h"

#include <cstdio>

#include "pico/stdlib.h"

InputManager::InputManager()
    : menuPressed(false),
      nextFieldPressed(false),
      charsetPressed(false) {
    // Initialize fixed pins
    gpio_init(MENU_PIN);
    gpio_set_dir(MENU_PIN, GPIO_IN);
    gpio_pull_up(MENU_PIN);

    gpio_init(NEXT_FIELD_PIN);
    gpio_set_dir(NEXT_FIELD_PIN, GPIO_IN);
    gpio_pull_up(NEXT_FIELD_PIN);

    gpio_init(CHARSET_PIN);
    gpio_set_dir(CHARSET_PIN, GPIO_IN);
    gpio_pull_up(CHARSET_PIN);
}

// Read raw button state (pressed = true)
bool InputManager::readButton(uint gpio) {
    return !gpio_get(gpio);
}

// Update a button flag
void InputManager::handleButton(uint gpio, volatile bool &flag) {
    flag = readButton(gpio);
}

// FreeRTOS task entry
void InputManager::taskEntry(void *pvParameters) {
    auto *self = static_cast<InputManager *>(pvParameters);
    self->inputTask();
}

// Input polling loop
[[noreturn]] void InputManager::inputTask() {
    bool lastMenu = false;
    bool lastNext = false;
    bool lastCharset = false;

    while (true) {
        bool currentMenu = readButton(MENU_PIN);
        bool currentNext = readButton(NEXT_FIELD_PIN);
        bool currentCharset = readButton(CHARSET_PIN);

        // Detect changes
        if (currentMenu != lastMenu) {
            printf("[MENU] %s\n", currentMenu ? "PRESSED" : "RELEASED");
            menuPressed = currentMenu;
            lastMenu = currentMenu;
        }

        if (currentNext != lastNext) {
            printf("[NEXT] %s\n", currentNext ? "PRESSED" : "RELEASED");
            nextFieldPressed = currentNext;
            lastNext = currentNext;
        }

        if (currentCharset != lastCharset) {
            printf("[CHAR] %s\n", currentCharset ? "PRESSED" : "RELEASED");
            charsetPressed = currentCharset;
            lastCharset = currentCharset;
        }

        vTaskDelay(pdMS_TO_TICKS(250)); // 10 ms polling
    }
}


// Getters
bool InputManager::isMenuPressed() const {
    return menuPressed;
}

bool InputManager::isNextFieldPressed() const {
    return nextFieldPressed;
}

bool InputManager::isCharsetPressed() const {
    return charsetPressed;
}
