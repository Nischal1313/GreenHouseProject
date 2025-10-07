// inputManager.cpp - Fixed version with proper debouncing
#include "inputManager.h"
#include <cstdio>
#include "pico/stdlib.h"

InputManager::InputManager()
    : menuPressed(false),
      nextFieldPressed(false),
      charsetPressed(false),
      menuPressEvent(false),
      nextFieldPressEvent(false),
      charsetPressEvent(false) {
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

// FreeRTOS task entry
void InputManager::taskEntry(void *pvParameters) {
    auto *self = static_cast<InputManager *>(pvParameters);
    self->inputTask();
}

// Input polling loop with proper debouncing
[[noreturn]] void InputManager::inputTask() {
    // Button state tracking
    bool lastMenuState = false;
    bool lastNextState = false;
    bool lastCharsetState = false;

    // Debounce timers (in milliseconds)
    uint32_t menuDebounceTimer = 0;
    uint32_t nextDebounceTimer = 0;
    uint32_t charsetDebounceTimer = 0;

    const uint32_t DEBOUNCE_TIME = 50;  // 50ms debounce time
    const uint32_t POLL_INTERVAL = 10;   // 10ms polling interval

    while (true) {
        bool currentMenu = readButton(MENU_PIN);
        bool currentNext = readButton(NEXT_FIELD_PIN);
        bool currentCharset = readButton(CHARSET_PIN);

        // Handle MENU button with debouncing
        if (currentMenu != lastMenuState) {
            menuDebounceTimer = 0;
            lastMenuState = currentMenu;
        } else if (menuDebounceTimer < DEBOUNCE_TIME) {
            menuDebounceTimer += POLL_INTERVAL;
            if (menuDebounceTimer >= DEBOUNCE_TIME) {
                // Stable state achieved
                if (currentMenu && !menuPressed) {
                    // Rising edge detected (button just pressed)
                    printf("[MENU] PRESSED\n");
                    menuPressEvent = true;  // Set event flag
                } else if (!currentMenu && menuPressed) {
                    // Falling edge detected (button released)
                    printf("[MENU] RELEASED\n");
                    menuPressEvent = false;  // Clear event flag
                }
                menuPressed = currentMenu;
            }
        }

        // Handle NEXT_FIELD button with debouncing
        if (currentNext != lastNextState) {
            nextDebounceTimer = 0;
            lastNextState = currentNext;
        } else if (nextDebounceTimer < DEBOUNCE_TIME) {
            nextDebounceTimer += POLL_INTERVAL;
            if (nextDebounceTimer >= DEBOUNCE_TIME) {
                // Stable state achieved
                if (currentNext && !nextFieldPressed) {
                    // Rising edge detected
                    printf("[NEXT] PRESSED\n");
                    nextFieldPressEvent = true;
                } else if (!currentNext && nextFieldPressed) {
                    // Falling edge detected
                    printf("[NEXT] RELEASED\n");
                    nextFieldPressEvent = false;
                }
                nextFieldPressed = currentNext;
            }
        }

        // Handle CHARSET button with debouncing
        if (currentCharset != lastCharsetState) {
            charsetDebounceTimer = 0;
            lastCharsetState = currentCharset;
        } else if (charsetDebounceTimer < DEBOUNCE_TIME) {
            charsetDebounceTimer += POLL_INTERVAL;
            if (charsetDebounceTimer >= DEBOUNCE_TIME) {
                // Stable state achieved
                if (currentCharset && !charsetPressed) {
                    // Rising edge detected
                    printf("[CHAR] PRESSED\n");
                    charsetPressEvent = true;
                } else if (!currentCharset && charsetPressed) {
                    // Falling edge detected
                    printf("[CHAR] RELEASED\n");
                    charsetPressEvent = false;
                }
                charsetPressed = currentCharset;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL));
    }
}

// Get button press event (consumes the event)
bool InputManager::getMenuPressEvent() {
    if (menuPressEvent) {
        menuPressEvent = false;  // Consume the event
        return true;
    }
    return false;
}

bool InputManager::getNextFieldPressEvent() {
    if (nextFieldPressEvent) {
        nextFieldPressEvent = false;  // Consume the event
        return true;
    }
    return false;
}

bool InputManager::getCharsetPressEvent() {
    if (charsetPressEvent) {
        charsetPressEvent = false;  // Consume the event
        return true;
    }
    return false;
}

// Getters for button states (continuous state)
bool InputManager::isMenuPressed() const {
    return menuPressed;
}

bool InputManager::isNextFieldPressed() const {
    return nextFieldPressed;
}

bool InputManager::isCharsetPressed() const {
    return charsetPressed;
}
