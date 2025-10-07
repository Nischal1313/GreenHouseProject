// setCredentials.cpp - Fixed version with proper EEPROM handling
#include "setCredentials.h"
#include "mutexGuard.h"
#include "pico/time.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

static constexpr uint32_t DEBOUNCE_MS = 150;

SetCredentials::SetCredentials(Eeprom& eeprom, SemaphoreHandle_t eepromMutex)
    : eeprom(eeprom),
      eepromMutex(eepromMutex),
      currentField(CredentialField::WIFI_NAME),
      charsetMode(CharsetMode::LOWERCASE),
      currentCharIndex(0),
      lastButtonTime(make_timeout_time_us(0))
{
    printf("[SetCredentials] Constructor called.\n");

    // Initialize character sets
    charsets[0] = "abcdefghijklmnopqrstuvwxyz";
    charsets[1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    charsets[2] = "0123456789";

    // Initialize buffers as empty
    buffers[0] = "";
    buffers[1] = "";

    setChars();
    loadFromEEPROM();  // Load after setting up everything

    printf("[SetCredentials] Init complete. Field=%s, Charset=LOWERCASE\n",
           getCurrentFieldName());
}

void SetCredentials::setChars() {
    gpio_init(ENCODER_BUTTON_PIN);
    gpio_set_dir(ENCODER_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_BUTTON_PIN);
    lastButtonState = true;
}

bool SetCredentials::debounceButtonPressed() {
    bool pressed = !gpio_get(ENCODER_BUTTON_PIN);
    absolute_time_t now = get_absolute_time();

    // Edge detection with debounce
    if (pressed && !lastButtonState &&
        absolute_time_diff_us(lastButtonTime, now) > DEBOUNCE_MS * 1000) {
        lastButtonTime = now;
        lastButtonState = pressed;
        return true;
    }

    lastButtonState = pressed;
    return false;
}

bool SetCredentials::isValidString(const uint8_t* data, size_t len) const {
    if (len == 0) return false;

    // Check first byte - if it's 0xFF or 0x00, EEPROM is likely uninitialized
    if (data[0] == 0xFF || data[0] == 0x00) {
        // Check if entire block is 0xFF or 0x00
        bool allSame = true;
        for (size_t i = 1; i < len && i < FIELD_SIZE; i++) {
            if (data[i] != data[0]) {
                allSame = false;
                break;
            }
        }
        if (allSame) return false;  // Uninitialized EEPROM
    }

    // Check for valid ASCII printable characters
    for (size_t i = 0; i < len && i < FIELD_SIZE; i++) {
        if (data[i] == 0) break;  // Stop at null terminator

        // Must be printable ASCII (32-126)
        if (data[i] < 32 || data[i] > 126) {
            return false;
        }
    }

    return true;
}

void SetCredentials::loadFromEEPROM() {
    printf("[SetCredentials] Loading from EEPROM...\n");

    uint8_t tmp[FIELD_SIZE];
    MutexGuard lock(eepromMutex);

    // Load WiFi name
    memset(tmp, 0, sizeof(tmp));
    if (lock.owns_lock() && eeprom.readBlock(EEPROM_WIFI_NAME_ADDR, tmp, FIELD_SIZE)) {
        if (isValidString(tmp, FIELD_SIZE)) {
            // Find actual string length (stop at first null or end)
            size_t len = 0;
            for (size_t i = 0; i < FIELD_SIZE - 1; i++) {
                if (tmp[i] == 0) break;
                len++;
            }
            tmp[len] = '\0';  // Ensure null termination

            buffers[0] = reinterpret_cast<char*>(tmp);
            printf("[EEPROM] Loaded WiFi SSID: '%s' (len=%zu)\n", buffers[0].c_str(), buffers[0].size());
        } else {
            buffers[0] = "";
            printf("[EEPROM] WiFi SSID empty or invalid, starting fresh\n");
        }
    } else {
        buffers[0] = "";
        printf("[EEPROM] Failed to read WiFi SSID\n");
    }

    // Load WiFi password
    memset(tmp, 0, sizeof(tmp));
    if (lock.owns_lock() && eeprom.readBlock(EEPROM_WIFI_PASSWD_ADDR, tmp, FIELD_SIZE)) {
        if (isValidString(tmp, FIELD_SIZE)) {
            // Find actual string length
            size_t len = 0;
            for (size_t i = 0; i < FIELD_SIZE - 1; i++) {
                if (tmp[i] == 0) break;
                len++;
            }
            tmp[len] = '\0';

            buffers[1] = reinterpret_cast<char*>(tmp);
            printf("[EEPROM] Loaded WiFi Password: '%s' (len=%zu)\n", buffers[1].c_str(), buffers[1].size());
        } else {
            buffers[1] = "";
            printf("[EEPROM] WiFi Password empty or invalid, starting fresh\n");
        }
    } else {
        buffers[1] = "";
        printf("[EEPROM] Failed to read WiFi Password\n");
    }
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

CharsetMode SetCredentials::getCharsetMode() const {
    return charsetMode;
}

void SetCredentials::saveFieldToEEPROM(CredentialField field) {
    uint16_t offset = (field == CredentialField::WIFI_NAME) ?
                      EEPROM_WIFI_NAME_ADDR : EEPROM_WIFI_PASSWD_ADDR;
    int idx = static_cast<int>(field);

    const auto& buf = buffers[idx];

    // Don't save if buffer is empty (keep existing value in EEPROM)
    if (buf.empty()) {
        printf("[EEPROM] Skipping save for empty field %d\n", idx);
        return;
    }

    MutexGuard lock(eepromMutex);
    if (lock.owns_lock()) {
        uint8_t writeData[FIELD_SIZE];
        memset(writeData, 0, FIELD_SIZE);  // Clear entire buffer with zeros

        // Copy string data
        size_t copyLen = std::min(buf.size(), static_cast<size_t>(FIELD_SIZE - 1));
        memcpy(writeData, buf.c_str(), copyLen);
        writeData[copyLen] = '\0';  // Explicit null termination

        // Write the entire block including the null terminator and padding zeros
        if (eeprom.writeBlock(offset, writeData, FIELD_SIZE)) {
            printf("[EEPROM] Saved field %d: '%s' (len=%zu)\n", idx, buf.c_str(), copyLen);

            // Verify write by reading back
            uint8_t verifyData[FIELD_SIZE];
            if (eeprom.readBlock(offset, verifyData, FIELD_SIZE)) {
                if (memcmp(writeData, verifyData, FIELD_SIZE) == 0) {
                    printf("[EEPROM] Verify OK for field %d\n", idx);
                } else {
                    printf("[EEPROM] Verify FAILED for field %d\n", idx);
                }
            }
        } else {
            printf("[EEPROM] Failed to write field %d\n", idx);
        }
    }
}

void SetCredentials::rotateChar(int direction) {
    const auto& cs = charsets[static_cast<int>(charsetMode)];
    if (cs.empty()) return;

    int len = static_cast<int>(cs.size());
    int newIndex = static_cast<int>(currentCharIndex) + direction;

    // Wrap around properly
    while (newIndex < 0) newIndex += len;
    while (newIndex >= len) newIndex -= len;

    currentCharIndex = newIndex;
    printf("[SetCredentials] Rotated to '%c' (index=%zu)\n", cs[currentCharIndex], currentCharIndex);
}

void SetCredentials::confirmChar() {
    auto& buf = currentBuffer();

    // Check if we've reached the limit
    if (buf.size() >= FIELD_SIZE - 1) {
        printf("[SetCredentials] Buffer full (%zu chars), cannot add more\n", buf.size());
        return;
    }

    char c = getCurrentChar();
    buf.push_back(c);
    printf("[SetCredentials] Added '%c', buffer='%s' (len=%zu)\n", c, buf.c_str(), buf.size());
}

void SetCredentials::clearCurrentField() {
    auto& buf = currentBuffer();
    if (!buf.empty()) {
        printf("[SetCredentials] Clearing field %s: was '%s'\n",
               getCurrentFieldName(), buf.c_str());
        buf.clear();
    }
}

const char* SetCredentials::getCurrentBuffer() {
    // Check for encoder button press to add character
    if (debounceButtonPressed()) {
        printf("[SetCredentials] Encoder button pressed - confirming char.\n");
        confirmChar();
    }

    const auto& ref = buffers[static_cast<int>(currentField)];

    // Return current value or "EMPTY" if empty
    if (ref.empty()) {
        return "EMPTY";
    }

    return ref.c_str();
}

const char* SetCredentials::getCurrentFieldName() const {
    switch (currentField) {
        case CredentialField::WIFI_NAME:   return "WiFi SSID";
        case CredentialField::WIFI_PASSWD: return "WiFi Pass";
        default: return "Unknown";
    }
}

void SetCredentials::nextField() {
    // Save current field before switching
    saveFieldToEEPROM(currentField);

    // Switch field
    currentField = (currentField == CredentialField::WIFI_NAME) ?
                   CredentialField::WIFI_PASSWD : CredentialField::WIFI_NAME;

    // Reset character index for new field
    currentCharIndex = 0;

    printf("[SetCredentials] Switched to field: %s (current value: '%s')\n",
           getCurrentFieldName(), buffers[static_cast<int>(currentField)].c_str());
}

void SetCredentials::nextCharset() {
    int idx = static_cast<int>(charsetMode);
    charsetMode = static_cast<CharsetMode>((idx + 1) % 3);
    currentCharIndex = 0;  // Reset to first character of new charset

    const char* modeName =
        (charsetMode == CharsetMode::LOWERCASE) ? "LOWERCASE" :
        (charsetMode == CharsetMode::UPPERCASE) ? "UPPERCASE" :
        "NUMBERS";
    printf("[SetCredentials] Charset changed to: %s\n", modeName);
}

void SetCredentials::saveAllToEEPROM() {
    printf("[SetCredentials] Saving all fields to EEPROM...\n");
    saveFieldToEEPROM(CredentialField::WIFI_NAME);
    saveFieldToEEPROM(CredentialField::WIFI_PASSWD);
    printf("[SetCredentials] All fields saved\n");
}
