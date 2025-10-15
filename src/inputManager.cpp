#include "inputManager.h"
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "sensor_handler.h"

InputManager::InputManager()
  : mainMenu(true),
    ssidSelected(true),
    charsetModes({
      InputCharsetMode::CAPITAL, InputCharsetMode::NORMAL,
      InputCharsetMode::NUMERIC
    }),
    currentCharsetIndex(0) {
  menuButton = std::make_unique<GPIOPin>(MENU_PIN, GPIOMode::INPUT,
                                         GPIOPull::PULLUP, false,
                                         DEBOUNCE_MS);

  nextFieldButton = std::make_unique<GPIOPin>(
    NEXT_FIELD_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false,
    200);

  charsetButton = std::make_unique<GPIOPin>(
    CHARSET_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false,
    DEBOUNCE_MS);

  menuButton->setHoldTime(HOLD_MS);
  nextFieldButton->setHoldTime(HOLD_MS);
  charsetButton->setHoldTime(HOLD_MS);
}

void InputManager::taskEntry(void *pvParameters) {
  static_cast<InputManager *>(pvParameters)->inputTask();
}

[[noreturn]] void InputManager::inputTask() {
  while (true) {
    menuButton->update();
    nextFieldButton->update();
    charsetButton->update();

    if (menuButton->pressed()) {
      mainMenu = !mainMenu;
    }

    if (nextFieldButton->pressed()) {
      ssidSelected = !ssidSelected;
    }

    if (charsetButton->pressed()) {
      currentCharsetIndex = (currentCharsetIndex + 1) % charsetModes.
                            size();
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}
