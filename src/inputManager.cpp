#include "inputManager.h"
#include <cstdio>

InputManager::InputManager() {
  // Create and initialize GPIOPin buttons
  menuButton = std::make_unique<GPIOPin>(
      MENU_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, DEBOUNCE_MS);

  nextFieldButton = std::make_unique<GPIOPin>(
      NEXT_FIELD_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, DEBOUNCE_MS);

  charsetButton = std::make_unique<GPIOPin>(
      CHARSET_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, DEBOUNCE_MS);

  // Configure hold detection time (default = 1s)
  menuButton->setHoldTime(HOLD_MS);
  nextFieldButton->setHoldTime(HOLD_MS);
  charsetButton->setHoldTime(HOLD_MS);

  printf("[InputManager] Buttons initialized (pins %u, %u, %u)\n",
         MENU_PIN, NEXT_FIELD_PIN, CHARSET_PIN);
}

void InputManager::taskEntry(void *pvParameters) {
  const auto *self = static_cast<InputManager *>(pvParameters);
  self->inputTask();
}

[[noreturn]] void InputManager::inputTask() const {
  printf("[InputManager] Task started\n");

  while (true) {
    menuButton->update();
    nextFieldButton->update();
    charsetButton->update();

    // Debug print to confirm runtime operation
    if (menuButton->pressed())  printf("[BTN] MENU pressed\n");
    if (nextFieldButton->pressed()) printf("[BTN] NEXT pressed\n");
    if (charsetButton->pressed()) printf("[BTN] CHARSET pressed\n");

    if (menuButton->held())  printf("[BTN] MENU held\n");
    if (nextFieldButton->held()) printf("[BTN] NEXT held\n");
    if (charsetButton->held()) printf("[BTN] CHARSET held\n");

    vTaskDelay(pdMS_TO_TICKS(10));  // fast and responsive
  }
}

// --- Press Event Getters ---
bool InputManager::getMenuPressEvent() const  { return menuButton->pressed(); }
bool InputManager::getNextFieldPressEvent() const { return nextFieldButton->pressed(); }
bool InputManager::getCharsetPressEvent() const   { return charsetButton->pressed(); }

// --- Hold Event Getters ---
bool InputManager::getMenuHoldEvent() const  { return menuButton->held(); }
bool InputManager::getNextFieldHoldEvent() const { return nextFieldButton->held(); }
bool InputManager::getCharsetHoldEvent() const   { return charsetButton->held(); }
