#include "inputManager.h"
#include <cstdio>

InputManager::InputManager() {
    // Create GPIO buttons with default settings (INPUT, PULLUP, debounce=50ms)
    menuButton = std::make_unique<GPIOPin>(
        MENU_PIN,
        GPIOMode::INPUT,
        GPIOPull::PULLUP,
        false,  // not inverted (active low handled by GPIOPin)
        150      // 50ms debounce
    );

    nextFieldButton = std::make_unique<GPIOPin>(
        NEXT_FIELD_PIN,
        GPIOMode::INPUT,
        GPIOPull::PULLUP,
        false,
        150
    );

    charsetButton = std::make_unique<GPIOPin>(
        CHARSET_PIN,
        GPIOMode::INPUT,
        GPIOPull::PULLUP,
        false,
        150
    );
    printf("[InputManager] Initialized with GPIOPin buttons\n");
}

// FreeRTOS task entry
void InputManager::taskEntry(void *pvParameters) {
    const auto *self = static_cast<InputManager *>(pvParameters);
    self->inputTask();
}

// Input polling loop - just call update() on each button
[[noreturn]] void InputManager::inputTask() const {
    printf("[InputManager] Task started\n");

    while (true) {
        // Update all buttons (handles debouncing internally)
        menuButton->update();
        nextFieldButton->update();
        charsetButton->update();

        // Poll every 5ms for responsive input
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Get button press events (consumed on read)
bool InputManager::getMenuPressEvent() const {
    return menuButton->pressed();
}

bool InputManager::getNextFieldPressEvent() const {
    return nextFieldButton->pressed();
}

bool InputManager::getCharsetPressEvent() const {
    return charsetButton->pressed();
}