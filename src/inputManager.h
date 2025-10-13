#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "FreeRTOS.h"
#include "task.h"
#include "gpio/gpio_pin.h"
#include <memory>

class InputManager {
public:
  InputManager();

  static void taskEntry(void *pvParameters);
  [[noreturn]] void inputTask() const;

  // Edge-triggered events
  [[nodiscard]] bool getMenuPressEvent() const;
  [[nodiscard]] bool getNextFieldPressEvent() const;
  [[nodiscard]] bool getCharsetPressEvent() const;

  // Hold events
  [[nodiscard]] bool getMenuHoldEvent() const;
  [[nodiscard]] bool getNextFieldHoldEvent() const;
  [[nodiscard]] bool getCharsetHoldEvent() const;

private:
  std::unique_ptr<GPIOPin> menuButton;
  std::unique_ptr<GPIOPin> nextFieldButton;
  std::unique_ptr<GPIOPin> charsetButton;

  static constexpr uint MENU_PIN = 7;
  static constexpr uint NEXT_FIELD_PIN = 8;
  static constexpr uint CHARSET_PIN = 9;

  static constexpr uint DEBOUNCE_MS = 150;
  static constexpr uint HOLD_MS = 1000;
};

#endif
