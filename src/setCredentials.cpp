#include "setCredentials.h"
#include "mutexGuard.h"
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include "pico/time.h"

bool CredentialValidator::isValidString(uint8_t const *pDataP,
                                        size_t const lenP,
                                        size_t const fieldSizeP)
{
    bool result{true};

    if (pDataP[0] == 0xFF || pDataP[0] == 0x00)
    {
        bool allSame{true};
        for (size_t i{1}; i < lenP && i < fieldSizeP; i++)
        {
            if (pDataP[i] != pDataP[0])
            {
                allSame = false;
                break;
            }
        }
        result = !allSame;
    }
    else
    {
        for (size_t i{0}; i < lenP && i < fieldSizeP; i++)
        {
            if (pDataP[i] == 0)
            {
                break;
            }
            if (pDataP[i] < 32 || pDataP[i] > 126)
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

SetCredentials::SetCredentials(Eeprom &rEepromP,
                               SemaphoreHandle_t eepromMutexP)
    : rEepromM{rEepromP},
      eepromMutexM{eepromMutexP},
      currentFieldM{CredentialField::WIFI_NAME},
      charsetModeM{CharsetMode::LOWERCASE},
      currentCharIndexM{0}
{
    charsetsM[0] = "abcdefghijklmnopqrstuvwxyz";
    charsetsM[1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    charsetsM[2] = "0123456789,-/?+:.<>|#!%()[]{}";
    buffersM[0].clear();
    buffersM[1].clear();
    loadFromEEPROM();
}

void SetCredentials::loadFromEEPROM()
{
    uint8_t tmp[FIELD_SIZE]{0};
    MutexGuard lock{eepromMutexM};

    if (lock.owns_lock())
    {
        auto loadField = [&](CredentialField const fieldP,
                             uint16_t const addressP, int const indexP)
        {
            printf("[EEPROM] Loading field %s from address 0x%04X\n",
                   fieldP == CredentialField::WIFI_NAME ? "SSID" : "Password",
                   addressP);

            if (!rEepromM.readBlock(addressP, tmp, FIELD_SIZE))
            {
                printf("[EEPROM] ✗ Failed to read field at 0x%04X\n", addressP);
                buffersM[indexP].clear();
                return;
            }

            if (!CredentialValidator::isValidString(
                    tmp, FIELD_SIZE, FIELD_SIZE))
            {
                buffersM[indexP].clear();
                return;
            }

            tmp[FIELD_SIZE - 1] = '\0';
            buffersM[indexP] = std::string(reinterpret_cast<char *>(tmp));
            if (buffersM[indexP].size() > MAX_CREDENTIAL_LENGTH)
            {
                buffersM[indexP] = buffersM[indexP].substr(0, MAX_CREDENTIAL_LENGTH);
            }
        };

        loadField(CredentialField::WIFI_NAME, EEPROM_WIFI_NAME_ADDR, 0);
        loadField(CredentialField::WIFI_PASSWD, EEPROM_WIFI_PASSWD_ADDR, 1);
    }
}

void SetCredentials::saveFieldToEEPROM(CredentialField fieldP) const
{
    MutexGuard lock{eepromMutexM};
    if (lock.owns_lock())
    {
        uint16_t const offset{(fieldP == CredentialField::WIFI_NAME)
                                  ? EEPROM_WIFI_NAME_ADDR
                                  : EEPROM_WIFI_PASSWD_ADDR};

        auto const &buf{buffersM[static_cast<int>(fieldP)]};
        uint8_t writeData[FIELD_SIZE]{0};
        size_t const len{std::min(buf.size(), static_cast<size_t>(FIELD_SIZE - 1))};
        memcpy(writeData, buf.c_str(), len);

        if (!rEepromM.writeBlock(offset, writeData, FIELD_SIZE))
        {
            printf("[SetCredentials] Failed to write field %d to EEPROM\n",
                   static_cast<int>(fieldP));
        }
    }
}

char SetCredentials::getCurrentChar() const
{
    char result{'a'};
    auto const &cs{charsetsM[static_cast<int>(charsetModeM)]};
    if (!cs.empty() && currentCharIndexM < cs.size())
    {
        result = cs[currentCharIndexM];
    }
    return result;
}

std::string &SetCredentials::currentBuffer()
{
    return buffersM[static_cast<int>(currentFieldM)];
}

std::string const &SetCredentials::currentBuffer() const
{
    return buffersM[static_cast<int>(currentFieldM)];
}

CharsetMode SetCredentials::getCharsetMode() const
{
    return charsetModeM;
}

void SetCredentials::rotateChar(int const directionP)
{
    auto const &cs{charsetsM[static_cast<int>(charsetModeM)]};
    if (!cs.empty())
    {
        int const len{static_cast<int>(cs.size())};
        currentCharIndexM = (currentCharIndexM + len + directionP) % len;
    }
}

void SetCredentials::confirmChar()
{
    auto &buf{currentBuffer()};
    char const c{getCurrentChar()};
    if (buf.size() >= MAX_CREDENTIAL_LENGTH)
    {
        buf.clear();
    }
    buf.push_back(c);
    saveFieldToEEPROM(currentFieldM);
}

void SetCredentials::clearCurrentField()
{
    auto &buf{currentBuffer()};
    if (!buf.empty())
    {
        buf.clear();
    }
}

char const *SetCredentials::getCurrentBuffer()
{
    auto const &ref{buffersM[static_cast<int>(currentFieldM)]};
    return ref.empty() ? "EMPTY" : ref.c_str();
}

char const *SetCredentials::getCurrentFieldName() const
{
    char const *result{"Unknown"};
    switch (currentFieldM)
    {
        case CredentialField::WIFI_NAME:
            result = "ssid";
            break;
        case CredentialField::WIFI_PASSWD:
            result = "passwd";
            break;
        default:
            break;
    }
    return result;
}

void SetCredentials::nextField()
{
    currentFieldM = (currentFieldM == CredentialField::WIFI_NAME)
                        ? CredentialField::WIFI_PASSWD
                        : CredentialField::WIFI_NAME;
    currentCharIndexM = 0;
}

void SetCredentials::nextCharset()
{
    loadFromEEPROM();
    charsetModeM = static_cast<CharsetMode>(
        (static_cast<int>(charsetModeM) + 1) % 3);
    currentCharIndexM = 0;
}
