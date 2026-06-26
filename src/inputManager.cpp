#include "inputManager.h"
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "sensor_handler.h"

InputManager::InputManager()
    : mainMenuM{true},
      ssidSelectedM{true},
      charsetModesM{InputCharsetMode::CAPITAL, InputCharsetMode::NORMAL,
                    InputCharsetMode::NUMERIC},
      currentCharsetIndexM{0},
      pMenuButtonM{std::make_unique<GPIOPin>(MENU_PIN, GPIOMode::INPUT,
                                             GPIOPull::PULLUP, false,
                                             DEBOUNCE_MS)},
      pNextFieldButtonM{std::make_unique<GPIOPin>(
          NEXT_FIELD_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false, 200)},
      pCharsetButtonM{std::make_unique<GPIOPin>(
          CHARSET_PIN, GPIOMode::INPUT, GPIOPull::PULLUP, false,
          DEBOUNCE_MS)}
{
    pMenuButtonM->setHoldTime(HOLD_MS);
    pNextFieldButtonM->setHoldTime(HOLD_MS);
    pCharsetButtonM->setHoldTime(HOLD_MS);
}

void InputManager::taskEntry(void *pvParametersP)
{
    static_cast<InputManager *>(pvParametersP)->inputTask();
}

[[noreturn]] void InputManager::inputTask()
{
    while (true)
    {
        pMenuButtonM->update();
        pNextFieldButtonM->update();
        pCharsetButtonM->update();

        if (pMenuButtonM->pressed())
        {
            mainMenuM = !mainMenuM;
        }

        if (pNextFieldButtonM->pressed())
        {
            ssidSelectedM = !ssidSelectedM;
        }

        if (pCharsetButtonM->pressed())
        {
            currentCharsetIndexM =
                (currentCharsetIndexM + 1) % charsetModesM.size();
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
