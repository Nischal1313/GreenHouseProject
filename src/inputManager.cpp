#include "inputManager.h"
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"

InputManager::InputManager()
  : mainMenu(true),
    ssidSelected(true),
    charsetModes({InputCharsetMode::CAPITAL, InputCharsetMode::NORMAL, InputCharsetMode::NUMERIC}),
    currentCharsetIndex(0)
{
  menuButton = std::make_unique<GPIOPin>(MENU_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, DEBOUNCE_MS);
  nextFieldButton = std::make_unique<GPIOPin>(NEXT_FIELD_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, 200);
  charsetButton = std::make_unique<GPIOPin>(CHARSET_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, DEBOUNCE_MS);

  menuButton->setHoldTime(HOLD_MS);
  nextFieldButton->setHoldTime(HOLD_MS);
  charsetButton->setHoldTime(HOLD_MS);

  printf("[InputManager] Initialized. MENU=%u, NEXT=%u, CHAR=%u\n",
         MENU_PIN, NEXT_FIELD_PIN, CHARSET_PIN);
}

void InputManager::taskEntry(void *pvParameters) {
  static_cast<InputManager *>(pvParameters)->inputTask();
}

[[noreturn]] void InputManager::inputTask() {
  printf("[InputManager] Task started\n");
  while (true) {
    menuButton->update();
    nextFieldButton->update();
    charsetButton->update();

    if (menuButton->pressed()) {
      mainMenu = !mainMenu;
      printf("[InputManager] mainMenu toggled -> %d\n", mainMenu);
    }

    if (nextFieldButton->pressed()) {
      ssidSelected = !ssidSelected;
      printf("[InputManager] ssidSelected toggled -> %d\n", ssidSelected);
    }

    if (charsetButton->pressed()) {
      currentCharsetIndex = (currentCharsetIndex + 1) % charsetModes.size();
      printf("[InputManager] Charset cycled -> %zu\n", currentCharsetIndex);
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}