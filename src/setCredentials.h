#pragma once

#include <string>
#include "eeprom/eeprom.h"
#include "FreeRTOS.h"
#include "semphr.h"

enum class CredentialField
{
    WIFI_NAME,
    WIFI_PASSWD
};

enum class CharsetMode
{
    LOWERCASE,
    UPPERCASE,
    NUMBERS
};

class CredentialValidator
{
public:
    static bool isValidString(uint8_t const *pDataP, size_t lenP,
                              size_t fieldSizeP);
};

class SetCredentials
{
public:
    explicit SetCredentials(Eeprom &rEepromP,
                            SemaphoreHandle_t eepromMutexP);

    void nextField();

    [[nodiscard]] char const *getCurrentFieldName() const;

    [[nodiscard]] CredentialField getCurrentField() const
    {
        return currentFieldM;
    }

    void nextCharset();

    [[nodiscard]] CharsetMode getCharsetMode() const;

    [[nodiscard]] char getCurrentChar() const;

    void rotateChar(int directionP);

    void confirmChar();

    void clearCurrentField();

    char const *getCurrentBuffer();

    void saveAllToEEPROM() const;

    [[nodiscard]] char const *getWifiSSID() const
    {
        return buffersM[0].c_str();
    }

    [[nodiscard]] char const *getWifiPassword() const
    {
        return buffersM[1].c_str();
    }

private:
    static constexpr uint16_t EEPROM_WIFI_NAME_ADDR{0x0500};
    static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR{0x0600};
    static constexpr uint16_t FIELD_SIZE{32};
    static constexpr uint MAX_CREDENTIAL_LENGTH{15};
    static constexpr uint EEPROM_SLEEP_MS{20};

    Eeprom &rEepromM;
    SemaphoreHandle_t eepromMutexM;
    CredentialField currentFieldM;
    CharsetMode charsetModeM;
    size_t currentCharIndexM;
    std::string buffersM[2];
    std::string charsetsM[3];

    void loadFromEEPROM();

    void saveFieldToEEPROM(CredentialField fieldP) const;

    std::string &currentBuffer();

    [[nodiscard]] std::string const &currentBuffer() const;
};
