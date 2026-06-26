extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
}

#include "setCredentials.h"
#include "displayMenu.h"
#include <cstdarg>
#include <cstring>
#include "pico/stdio.h"

DisplayManager::DisplayManager(std::shared_ptr<Debug> pDebugP,
                               std::shared_ptr<ssd1306os> pOledP,
                               std::shared_ptr<RotaryEncoder> const &pEncoderPtrP)
    : pOLedM{std::move(pOledP)},
      pDebugM{std::move(pDebugP)},
      pEncoderM{pEncoderPtrP},
      pParamsM{nullptr},
      menuStateM{MenuState::MAIN},
      prevSsidSelectedM{true},
      prevCharsetChangedM{false}
{
}

void DisplayManager::setParams(DisplayParams *pDisplayParamsP)
{
    if (!pDisplayParamsP)
    {
        printf("[DisplayManager] ERROR: displayParams is NULL!\n");
    }
    else
    {
        pParamsM = pDisplayParamsP;
    }
}

MenuState DisplayManager::getCurrentMenuState() const
{
    return menuStateM;
}

[[noreturn]] void DisplayManager::displayTask()
{
    while (true)
    {
        if (!pParamsM || !pParamsM->pInputManagerM)
        {
            printf("[DisplayManager] Waiting for valid params...\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        bool const inMainMenu{pParamsM->pInputManagerM->isMainMenu()};

        if (inMainMenu)
        {
            drawMainMenu();
        }
        else
        {
            handleWifiMenuButtons();
            drawWifiMenu();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void DisplayManager::drawMainMenu() const
{
    pOLedM->fill(0);

    char buf[128]{};
    auto const [temperatureM, humidityM, co2M, fanSpeedM, valveOpenM, targetCo2M] =
        pParamsM->pSensorHandlerM->getReadings();

    snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2M);
    pOLedM->text(buf, 2, 5);

    snprintf(buf, sizeof(buf), "Set: %d ppm", targetCo2M);
    pOLedM->text(buf, 2, 15);

    snprintf(buf, sizeof(buf), "Temp: %.1fC", temperatureM);
    pOLedM->text(buf, 2, 25);

    snprintf(buf, sizeof(buf), "Hum: %.1f%%", humidityM);
    pOLedM->text(buf, 2, 35);

    snprintf(buf, sizeof(buf), "Fan: %.0f%%", fanSpeedM);
    pOLedM->text(buf, 2, 45);

    snprintf(buf, sizeof(buf), "Valve: %s",
             valveOpenM ? "OPEN" : "CLOSED");
    pOLedM->text(buf, 2, 55);

    pOLedM->show();
}

void DisplayManager::drawWifiMenu()
{
    pOLedM->fill(0);

    auto *pCred{pParamsM->pCredentialsM};
    char buf[128]{};

    snprintf(buf, sizeof(buf), "|%s|", pCred->getCurrentFieldName());
    pOLedM->text(buf, 50, 2);

    snprintf(buf, sizeof(buf), "%s", pCred->getCurrentBuffer());
    pOLedM->text(buf, 0, 12);

    auto const mode{pCred->getCharsetMode()};
    char const *charsetName{
        (mode == CharsetMode::LOWERCASE)
            ? "a b z"
            : (mode == CharsetMode::UPPERCASE)
                ? "A B Z"
                : (mode == CharsetMode::NUMBERS)
                    ? "1 %  _ <"
                    : "undef"};
    snprintf(buf, sizeof(buf), "Chars: %s", charsetName);
    pOLedM->text(buf, 2, 22);

    char curChar{pCred->getCurrentChar()};
    snprintf(buf, sizeof(buf), "-> %c <-", curChar);
    pOLedM->text(buf, 55, 32);

    pOLedM->show();
}

void DisplayManager::handleWifiMenuButtons()
{
    bool const currentSsidSelected{pParamsM->pInputManagerM->isSSIDSelected()};
    if (currentSsidSelected != prevSsidSelectedM)
    {
        prevSsidSelectedM = currentSsidSelected;
        pParamsM->pCredentialsM->nextField();
    }

    auto const inputCharset{pParamsM->pInputManagerM->getCharsetMode()};
    auto const credCharset{pParamsM->pCredentialsM->getCharsetMode()};

    CharsetMode targetCharset;
    switch (inputCharset)
    {
        case InputCharsetMode::CAPITAL:
            targetCharset = CharsetMode::UPPERCASE;
            break;
        case InputCharsetMode::NORMAL:
            targetCharset = CharsetMode::LOWERCASE;
            break;
        case InputCharsetMode::NUMERIC:
            targetCharset = CharsetMode::NUMBERS;
            break;
        default:
            targetCharset = CharsetMode::LOWERCASE;
            break;
    }

    if (credCharset != targetCharset)
    {
        while (pParamsM->pCredentialsM->getCharsetMode() != targetCharset)
        {
            pParamsM->pCredentialsM->nextCharset();
        }
    }

    if (pEncoderM->rotatedCW())
    {
        pParamsM->pCredentialsM->rotateChar(1);
    }

    if (pEncoderM->rotatedCCW())
    {
        pParamsM->pCredentialsM->rotateChar(-1);
    }

    if (pEncoderM->buttonPressed())
    {
        pParamsM->pCredentialsM->confirmChar();
    }

    if (pEncoderM->buttonHeld())
    {
        pParamsM->pCredentialsM->clearCurrentField();
    }
}

void DisplayManager::changeMenu()
{
    menuStateM = (menuStateM == MenuState::MAIN)
                    ? MenuState::WIFI
                    : MenuState::MAIN;
}

void DisplayManager::taskEntry(void *pvParametersP)
{
    auto *self{static_cast<DisplayManager *>(pvParametersP)};
    self->displayTask();
}
