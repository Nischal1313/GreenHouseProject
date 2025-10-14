#pragma once
#include <memory>
#include <vector>
#include "gpio/gpio_pin.h"

// Renamed to avoid collision with SetCredentials::CharsetMode
enum class InputCharsetMode { CAPITAL, NORMAL, NUMERIC };

class InputManager {
public:
  InputManager();

  static void taskEntry(void *pvParameters);

  [[noreturn]] void inputTask();

  // --- Getters ---
  [[nodiscard]] bool isMainMenu() const { return mainMenu; }
  [[nodiscard]] bool isSSIDSelected() const { return ssidSelected; }
  [[nodiscard]] InputCharsetMode getCharsetMode() const { return charsetModes[currentCharsetIndex]; }

private:
  // GPIOs
  std::unique_ptr<GPIOPin> menuButton;
  std::unique_ptr<GPIOPin> nextFieldButton;
  std::unique_ptr<GPIOPin> charsetButton;

  // Internal states
  bool mainMenu;
  bool ssidSelected;

  std::vector<InputCharsetMode> charsetModes;
  size_t currentCharsetIndex;

  static constexpr uint MENU_PIN = 7;
  static constexpr uint NEXT_FIELD_PIN = 8;
  static constexpr uint CHARSET_PIN = 9;

  static constexpr uint DEBOUNCE_MS = 150;
  static constexpr uint HOLD_MS = 1000;
};