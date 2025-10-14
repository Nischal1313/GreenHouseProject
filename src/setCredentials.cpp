#include "setCredentials.h"
#include "mutexGuard.h"
#include <cstring>
#include <cstdarg>
#include <algorithm>
#include "pico/time.h"

void SetCredentials::debugLog(const char* fmt, ...) const {
#ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

bool CredentialValidator::isValidString(const uint8_t* data, const size_t len, const size_t fieldSize) {
    if (data[0] == 0xFF || data[0] == 0x00) {
        bool allSame = true;
        for (size_t i = 1; i < len && i < fieldSize; i++) {
            if (data[i] != data[0]) { allSame = false; break; }
        }
        return !allSame;  // all same = invalid
    }

    for (size_t i = 0; i < len && i < fieldSize; i++) {
        if (data[i] == 0) break;
        if (data[i] < 32 || data[i] > 126) return false;
    }
    return true;
}

SetCredentials::SetCredentials(Eeprom& eeprom, const SemaphoreHandle_t eepromMutex)
    : eeprom(eeprom),
      eepromMutex(eepromMutex),
      currentField(CredentialField::WIFI_NAME),
      charsetMode(CharsetMode::LOWERCASE),
      currentCharIndex(0)
{
    charsets[0] = "abcdefghijklmnopqrstuvwxyz";
    charsets[1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    charsets[2] = "0123456789,-/?+:.<>|#!%()[]{}";
    buffers[0].clear();
    buffers[1].clear();
    loadFromEEPROM();
}
//
// void SetCredentials::loadFromEEPROM() {
//     printf("loading[SetCredentials]\n]");
//     uint8_t tmp[FIELD_SIZE];
//     const MutexGuard lock(eepromMutex);
//
//     if (!lock.owns_lock()) {
//         debugLog("[EEPROM] Mutex acquisition failed during load.\n");
//         return;
//     }
//
//     auto loadField = [&](const CredentialField field, const uint16_t address, const int index) {
//         if (!eeprom.readBlock(address, tmp, FIELD_SIZE)) {
//             debugLog("[EEPROM] ✗ Failed to read %s\n",
//                      field == CredentialField::WIFI_NAME ? "SSID" : "Password");
//             buffers[index].clear();
//             return;
//         }
//
//         if (!CredentialValidator::isValidString(tmp, FIELD_SIZE, FIELD_SIZE)) {
//             debugLog("[EEPROM] ✗ Invalid string in %s\n",
//                      field == CredentialField::WIFI_NAME ? "SSID" : "Password");
//             buffers[index].clear();
//             return;
//         }
//
//         tmp[FIELD_SIZE - 1] = '\0';
//         buffers[index] = reinterpret_cast<char*>(tmp);
//         if (buffers[index].size() > MAX_CREDENTIAL_LENGTH)
//             buffers[index] = buffers[index].substr(0, MAX_CREDENTIAL_LENGTH);
//     };
//
//     loadField(CredentialField::WIFI_NAME, EEPROM_WIFI_NAME_ADDR, 0);
//     loadField(CredentialField::WIFI_PASSWD, EEPROM_WIFI_PASSWD_ADDR, 1);
//     printf("[loading]%d, %d\n", static_cast<int>(CredentialField::WIFI_PASSWD),
//                                  static_cast<int>(CredentialField::WIFI_NAME));
//
// }
//
// void SetCredentials::saveFieldToEEPROM(CredentialField field) const {
//     const uint16_t offset = (field == CredentialField::WIFI_NAME)
//         ? EEPROM_WIFI_NAME_ADDR
//         : EEPROM_WIFI_PASSWD_ADDR;
//
//     const auto& buf = buffers[static_cast<int>(field)];
//     MutexGuard lock(eepromMutex);
//
//     if (!lock.owns_lock()) {
//         debugLog("[EEPROM-setCreds] ✗ Mutex acquisition failed on save.\n");
//         return;
//     }
//
//     uint8_t writeData[FIELD_SIZE]{0};
//     const size_t len = std::min(buf.size(), static_cast<size_t>(FIELD_SIZE - 1));
//     memcpy(writeData, buf.c_str(), len);
//
//     if (!eeprom.writeBlock(offset, writeData, FIELD_SIZE))
//         debugLog("[EEPROM] ✗ Write failed for field %d\n", static_cast<int>(field));
//     printf("[saving]%d, %d\n", static_cast<int>(CredentialField::WIFI_PASSWD),
//                              static_cast<int>(CredentialField::WIFI_NAME));
//
//
// }

void SetCredentials::loadFromEEPROM() {
    printf("loading[SetCredentials]\n");
    uint8_t tmp[FIELD_SIZE];
    const MutexGuard lock(eepromMutex);

    if (!lock.owns_lock()) {
        debugLog("[EEPROM] Mutex acquisition failed during load.\n");
        return;
    }

    auto loadField = [&](const CredentialField field, const uint16_t address, const int index) {
        printf("[EEPROM] Loading field %s from address 0x%04X\n",
               field == CredentialField::WIFI_NAME ? "SSID" : "Password",
               address);

        if (!eeprom.readBlock(address, tmp, FIELD_SIZE)) {
            printf("[EEPROM] ✗ Failed to read field at 0x%04X\n", address);
            buffers[index].clear();
            return;
        }

        printf("[EEPROM] Raw bytes: ");
        for (size_t i = 0; i < FIELD_SIZE; i++) {
            printf("%02X ", tmp[i]);
        }
        printf("\n");

        if (!CredentialValidator::isValidString(tmp, FIELD_SIZE, FIELD_SIZE)) {
            printf("[EEPROM] ✗ Invalid string in field %s\n",
                   field == CredentialField::WIFI_NAME ? "SSID" : "Password");
            buffers[index].clear();
            return;
        }

        tmp[FIELD_SIZE - 1] = '\0';
        buffers[index] = std::string(reinterpret_cast<char*>(tmp));
        if (buffers[index].size() > MAX_CREDENTIAL_LENGTH)
            buffers[index] = buffers[index].substr(0, MAX_CREDENTIAL_LENGTH);

        printf("[EEPROM] Loaded '%s'\n", buffers[index].c_str());
    };

    loadField(CredentialField::WIFI_NAME, EEPROM_WIFI_NAME_ADDR, 0);
    loadField(CredentialField::WIFI_PASSWD, EEPROM_WIFI_PASSWD_ADDR, 1);
}

void SetCredentials::saveFieldToEEPROM(CredentialField field) const {
    const uint16_t offset = (field == CredentialField::WIFI_NAME)
        ? EEPROM_WIFI_NAME_ADDR
        : EEPROM_WIFI_PASSWD_ADDR;

    const auto& buf = buffers[static_cast<int>(field)];
    MutexGuard lock(eepromMutex);

    if (!lock.owns_lock()) {
        debugLog("[EEPROM-setCreds] ✗ Mutex acquisition failed on save.\n");
        return;
    }

    uint8_t writeData[FIELD_SIZE]{0};
    const size_t len = std::min(buf.size(), static_cast<size_t>(FIELD_SIZE - 1));
    memcpy(writeData, buf.c_str(), len);

    printf("[EEPROM] Saving field %s to address 0x%04X, data: ",
           field == CredentialField::WIFI_NAME ? "SSID" : "Password",
           offset);
    for (size_t i = 0; i < FIELD_SIZE; i++) {
        printf("%02X ", writeData[i]);
    }
    printf("\n");

    if (!eeprom.writeBlock(offset, writeData, FIELD_SIZE))
        printf("[EEPROM] ✗ Write failed for field %s at 0x%04X\n",
               field == CredentialField::WIFI_NAME ? "SSID" : "Password",
               offset);
    else
        printf("[EEPROM] Write OK for field %s\n",
               field == CredentialField::WIFI_NAME ? "SSID" : "Password");
}


char SetCredentials::getCurrentChar() const {
    const auto& cs = charsets[static_cast<int>(charsetMode)];
    if (cs.empty() || currentCharIndex >= cs.size()) return 'a';
    return cs[currentCharIndex];
}

std::string& SetCredentials::currentBuffer() {
    return buffers[static_cast<int>(currentField)];
}

const std::string& SetCredentials::currentBuffer() const {
    return buffers[static_cast<int>(currentField)];
}

CharsetMode SetCredentials::getCharsetMode() const { return charsetMode; }

void SetCredentials::rotateChar(const int direction) {
    const auto& cs = charsets[static_cast<int>(charsetMode)];
    if (cs.empty()) return;
    const int len = static_cast<int>(cs.size());
    currentCharIndex = (currentCharIndex + len + direction) % len;
}

void SetCredentials::confirmChar() {
    auto& buf = currentBuffer();
    const char c = getCurrentChar();
    if (buf.size() >= MAX_CREDENTIAL_LENGTH) buf.clear();
    buf.push_back(c);
    saveFieldToEEPROM(currentField);
}

void SetCredentials::clearCurrentField() {
    auto& buf = currentBuffer();
    if (!buf.empty()) {
        buf.clear();
        saveFieldToEEPROM(currentField);
    }
}

const char* SetCredentials::getCurrentBuffer() {
    const auto& ref = buffers[static_cast<int>(currentField)];
    return ref.empty() ? "EMPTY" : ref.c_str();
}

const char* SetCredentials::getCurrentFieldName() const {
    switch (currentField) {
        case CredentialField::WIFI_NAME: return "ssid";
        case CredentialField::WIFI_PASSWD: return "passwd";
        default: return "Unknown";
    }
}

void SetCredentials::nextField() {
    saveFieldToEEPROM(currentField);
    currentField = (currentField == CredentialField::WIFI_NAME)
        ? CredentialField::WIFI_PASSWD
        : CredentialField::WIFI_NAME;
    currentCharIndex = 0;
}

void SetCredentials::nextCharset() {
    loadFromEEPROM();
    charsetMode = static_cast<CharsetMode>((static_cast<int>(charsetMode) + 1) % 3);
    currentCharIndex = 0;
}

void SetCredentials::saveAllToEEPROM() const {
    saveFieldToEEPROM(CredentialField::WIFI_NAME);
    vTaskDelay(pdMS_TO_TICKS(EEPROM_SLEEP_MS));
    saveFieldToEEPROM(CredentialField::WIFI_PASSWD);
    vTaskDelay(pdMS_TO_TICKS(EEPROM_SLEEP_MS));
}
