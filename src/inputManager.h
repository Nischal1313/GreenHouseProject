#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "FreeRTOS.h"
#include "task.h"
#include "gpio/gpio_pin.h"
#include <memory>

class InputManager {
public:
  InputManager();

  // Start FreeRTOS task
  static void taskEntry(void *pvParameters);
  [[noreturn]] void inputTask() const;

  // Get button press events (edge-triggered, consumed on read)
  bool getMenuPressEvent() const;
  bool getNextFieldPressEvent() const;
  bool getCharsetPressEvent() const;

private:
  // GPIO buttons using GPIOPin class
  std::unique_ptr<GPIOPin> menuButton;
  std::unique_ptr<GPIOPin> nextFieldButton;
  std::unique_ptr<GPIOPin> charsetButton;

  // GPIO pin-numbers
  static constexpr uint MENU_PIN = 7;
  static constexpr uint NEXT_FIELD_PIN = 8;
  static constexpr uint CHARSET_PIN = 9;
};

#endif
