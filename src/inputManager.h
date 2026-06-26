#pragma once
#include <memory>
#include <vector>
#include "gpio/gpio_pin.h"

// Renamed to avoid collision with SetCredentials::CharsetMode
enum class InputCharsetMode { CAPITAL, NORMAL, NUMERIC };

class InputManager
{
public:
    InputManager();

    static void taskEntry(void *pvParametersP);

    [[noreturn]] void inputTask();

    // --- Getters ---
    [[nodiscard]] bool isMainMenu() const
    {
        return mainMenuM;
    }

    [[nodiscard]] bool isSSIDSelected() const
    {
        return ssidSelectedM;
    }

    [[nodiscard]] InputCharsetMode getCharsetMode() const
    {
        return charsetModesM[currentCharsetIndexM];
    }

private:
    // GPIOs
    std::unique_ptr<GPIOPin> pMenuButtonM;
    std::unique_ptr<GPIOPin> pNextFieldButtonM;
    std::unique_ptr<GPIOPin> pCharsetButtonM;

    // Internal states
    bool mainMenuM;
    bool ssidSelectedM;

    std::vector<InputCharsetMode> charsetModesM;
    size_t currentCharsetIndexM;

    static constexpr uint MENU_PIN{7};
    static constexpr uint NEXT_FIELD_PIN{8};
    static constexpr uint CHARSET_PIN{9};

    static constexpr uint DEBOUNCE_MS{150};
    static constexpr uint HOLD_MS{1000};
};
