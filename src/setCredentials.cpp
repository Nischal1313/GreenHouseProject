// setCredentials.cpp - Fixed version with 12-char limit and auto-reset on 13th char
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
    printf("\n[SetCredentials] ===== CONSTRUCTOR START =====\n");

    // Initialize character sets
    charsets[0] = "abcdefghijklmnopqrstuvwxyz";
    charsets[1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    charsets[2] = "0123456789";

    // Initialize buffers as empty
    buffers[0] = "";
    buffers[1] = "";

    setChars();

    printf("[SetCredentials] Loading saved credentials from EEPROM...\n");
    loadFromEEPROM();  // Load after setting up everything

    printf("[SetCredentials] Constructor complete. Field=%s, Charset=LOWERCASE\n",
           getCurrentFieldName());
    printf("[SetCredentials] Loaded SSID: '%s' (len=%zu)\n",
           buffers[0].empty() ? "EMPTY" : buffers[0].c_str(), buffers[0].size());
    printf("[SetCredentials] Loaded Password: '%s' (len=%zu)\n",
           buffers[1].empty() ? "EMPTY" : buffers[1].c_str(), buffers[1].size());
    printf("[SetCredentials] ===== CONSTRUCTOR END =====\n\n");
}

void SetCredentials::setChars() {
    gpio_init(ENCODER_BUTTON_PIN);
    gpio_set_dir(ENCODER_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_BUTTON_PIN);
    lastButtonState = true;
    printf("[SetCredentials] Encoder button initialized on pin %d\n", ENCODER_BUTTON_PIN);
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
    if (len == 0) {
        printf("[EEPROM] isValidString: length is 0\n");
        return false;
    }

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
        if (allSame) {
            printf("[EEPROM] isValidString: Block is uninitialized (all 0x%02X)\n", data[0]);
            return false;  // Uninitialized EEPROM
        }
    }

    // Check for valid ASCII printable characters
    size_t validChars = 0;
    for (size_t i = 0; i < len && i < FIELD_SIZE; i++) {
        if (data[i] == 0) {
            printf("[EEPROM] isValidString: Found null terminator at position %zu\n", i);
            break;  // Stop at null terminator
        }

        // Must be printable ASCII (32-126)
        if (data[i] < 32 || data[i] > 126) {
            printf("[EEPROM] isValidString: Invalid char 0x%02X at position %zu\n", data[i], i);
            return false;
        }
        validChars++;
    }

    printf("[EEPROM] isValidString: Valid string with %zu characters\n", validChars);
    return validChars > 0;
}

void SetCredentials::loadFromEEPROM() {
    printf("\n[SetCredentials] ===== LOADING FROM EEPROM =====\n");

    uint8_t tmp[FIELD_SIZE];
    MutexGuard lock(eepromMutex);

    // Load WiFi name
    printf("[EEPROM] Reading WiFi SSID from address 0x%04X...\n", EEPROM_WIFI_NAME_ADDR);
    memset(tmp, 0, sizeof(tmp));

    if (lock.owns_lock() && eeprom.readBlock(EEPROM_WIFI_NAME_ADDR, tmp, FIELD_SIZE)) {
        // Debug: Print raw bytes read
        printf("[EEPROM] Raw SSID bytes: ");
        for(int i = 0; i < 16; i++) {
            printf("%02X ", tmp[i]);
        }
        printf("...\n");

        if (isValidString(tmp, FIELD_SIZE)) {
            // Find actual string length (stop at first null or end)
            size_t len = 0;
            for (size_t i = 0; i < FIELD_SIZE - 1 && i < MAX_CREDENTIAL_LENGTH; i++) {
                if (tmp[i] == 0) break;
                len++;
            }
            tmp[len] = '\0';  // Ensure null termination

            buffers[0] = reinterpret_cast<char*>(tmp);

            // Truncate if loaded string exceeds max length (shouldn't happen but safety check)
            if (buffers[0].size() > MAX_CREDENTIAL_LENGTH) {
                printf("[EEPROM] ⚠ Loaded SSID exceeds max length (%zu > %d), truncating\n",
                       buffers[0].size(), MAX_CREDENTIAL_LENGTH);
                buffers[0] = buffers[0].substr(0, MAX_CREDENTIAL_LENGTH);
            }

            printf("[EEPROM] ✓ Loaded WiFi SSID: '%s' (len=%zu)\n", buffers[0].c_str(), buffers[0].size());
        } else {
            buffers[0] = "";
            printf("[EEPROM] ✗ WiFi SSID empty or invalid, starting fresh\n");
        }
    } else {
        buffers[0] = "";
        printf("[EEPROM] ✗ Failed to read WiFi SSID from EEPROM\n");
    }

    // Load WiFi password
    printf("[EEPROM] Reading WiFi Password from address 0x%04X...\n", EEPROM_WIFI_PASSWD_ADDR);
    memset(tmp, 0, sizeof(tmp));

    if (lock.owns_lock() && eeprom.readBlock(EEPROM_WIFI_PASSWD_ADDR, tmp, FIELD_SIZE)) {
        // Debug: Print raw bytes read
        printf("[EEPROM] Raw Password bytes: ");
        for(int i = 0; i < 16; i++) {
            printf("%02X ", tmp[i]);
        }
        printf("...\n");

        if (isValidString(tmp, FIELD_SIZE)) {
            // Find actual string length
            size_t len = 0;
            for (size_t i = 0; i < FIELD_SIZE - 1 && i < MAX_CREDENTIAL_LENGTH; i++) {
                if (tmp[i] == 0) break;
                len++;
            }
            tmp[len] = '\0';

            buffers[1] = reinterpret_cast<char*>(tmp);

            // Truncate if loaded string exceeds max length
            if (buffers[1].size() > MAX_CREDENTIAL_LENGTH) {
                printf("[EEPROM] ⚠ Loaded Password exceeds max length (%zu > %d), truncating\n",
                       buffers[1].size(), MAX_CREDENTIAL_LENGTH);
                buffers[1] = buffers[1].substr(0, MAX_CREDENTIAL_LENGTH);
            }

            printf("[EEPROM] ✓ Loaded WiFi Password: '%s' (len=%zu)\n", buffers[1].c_str(), buffers[1].size());
        } else {
            buffers[1] = "";
            printf("[EEPROM] ✗ WiFi Password empty or invalid, starting fresh\n");
        }
    } else {
        buffers[1] = "";
        printf("[EEPROM] ✗ Failed to read WiFi Password from EEPROM\n");
    }

    printf("[SetCredentials] ===== LOADING COMPLETE =====\n\n");
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
    const char* fieldName = (field == CredentialField::WIFI_NAME) ? "SSID" : "Password";

    const auto& buf = buffers[idx];

    printf("\n[EEPROM] ===== SAVING %s =====\n", fieldName);

    // Always save, even if empty (to clear old values)
    if (buf.empty()) {
        printf("[EEPROM] Buffer is empty, will clear EEPROM field\n");
    } else {
        printf("[EEPROM] Saving '%s' (len=%zu) to address 0x%04X\n",
               buf.c_str(), buf.size(), offset);
    }

    MutexGuard lock(eepromMutex);
    if (lock.owns_lock()) {
        uint8_t writeData[FIELD_SIZE];
        memset(writeData, 0, FIELD_SIZE);  // Clear entire buffer with zeros

        // Copy string data
        size_t copyLen = std::min(buf.size(), static_cast<size_t>(FIELD_SIZE - 1));
        if (copyLen > 0) {
            memcpy(writeData, buf.c_str(), copyLen);
        }
        writeData[copyLen] = '\0';  // Explicit null termination

        // Debug: Print what we're writing
        printf("[EEPROM] Writing bytes: ");
        for(int i = 0; i < 16; i++) {
            printf("%02X ", writeData[i]);
        }
        printf("...\n");

        // Write the entire block including the null terminator and padding zeros
        if (eeprom.writeBlock(offset, writeData, FIELD_SIZE)) {
            printf("[EEPROM] ✓ Write operation completed\n");

            // Add extra delay for EEPROM write cycle completion
            sleep_ms(10);  // Increased from 5ms in eeprom.cpp

            // Verify write by reading back
            uint8_t verifyData[FIELD_SIZE];
            memset(verifyData, 0xFF, FIELD_SIZE);  // Fill with 0xFF to detect read failures

            if (eeprom.readBlock(offset, verifyData, FIELD_SIZE)) {
                printf("[EEPROM] Verify read bytes: ");
                for(int i = 0; i < 16; i++) {
                    printf("%02X ", verifyData[i]);
                }
                printf("...\n");

                if (memcmp(writeData, verifyData, FIELD_SIZE) == 0) {
                    printf("[EEPROM] ✓✓ VERIFICATION SUCCESS for %s\n", fieldName);
                } else {
                    printf("[EEPROM] ✗✗ VERIFICATION FAILED for %s\n", fieldName);
                    printf("[EEPROM] Expected vs Read mismatch!\n");
                }
            } else {
                printf("[EEPROM] ✗ Verification read failed\n");
            }
        } else {
            printf("[EEPROM] ✗ Write operation FAILED for %s\n", fieldName);
        }
    } else {
        printf("[EEPROM] ✗ Could not acquire mutex for saving\n");
    }

    printf("[EEPROM] ===== SAVE COMPLETE =====\n\n");
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
    printf("[SetCredentials] Rotated to '%c' (index=%zu) in %s charset\n",
           cs[currentCharIndex], currentCharIndex,
           charsetMode == CharsetMode::LOWERCASE ? "lowercase" :
           charsetMode == CharsetMode::UPPERCASE ? "uppercase" : "numbers");
}

void SetCredentials::confirmChar() {
    auto& buf = currentBuffer();
    char c = getCurrentChar();

    // Check if we've reached the maximum length (12 characters)
    if (buf.size() >= MAX_CREDENTIAL_LENGTH) {
        printf("\n[SetCredentials] ===== AUTO-RESET TRIGGERED =====\n");
        printf("[SetCredentials] Buffer at max length (%zu chars): '%s'\n", buf.size(), buf.c_str());
        printf("[SetCredentials] Clearing field and starting with new char '%c'\n", c);

        // Clear the buffer
        buf.clear();

        // Add the new character as the first character
        buf.push_back(c);

        printf("[SetCredentials] New buffer after reset: '%s' (len=%zu)\n", buf.c_str(), buf.size());
        printf("[SetCredentials] Field %s has been reset and started fresh\n", getCurrentFieldName());
        printf("[SetCredentials] ===== AUTO-RESET COMPLETE =====\n\n");
    } else {
        // Normal case - just add the character
        buf.push_back(c);
        printf("[SetCredentials] Added '%c' to %s, buffer='%s' (len=%zu/%d)\n",
               c, getCurrentFieldName(), buf.c_str(), buf.size(), MAX_CREDENTIAL_LENGTH);
    }

    // Auto-save after each character addition or reset
    printf("[SetCredentials] Auto-saving after character operation...\n");
    saveFieldToEEPROM(currentField);
}

void SetCredentials::clearCurrentField() {
    auto& buf = currentBuffer();
    if (!buf.empty()) {
        printf("[SetCredentials] Clearing field %s: was '%s'\n",
               getCurrentFieldName(), buf.c_str());
        buf.clear();

        // Save the cleared field to EEPROM
        printf("[SetCredentials] Saving cleared field to EEPROM...\n");
        saveFieldToEEPROM(currentField);
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
    printf("\n[SetCredentials] ===== SWITCHING FIELD =====\n");
    printf("[SetCredentials] Current field: %s with value: '%s' (len=%zu/%d)\n",
           getCurrentFieldName(), buffers[static_cast<int>(currentField)].c_str(),
           buffers[static_cast<int>(currentField)].size(), MAX_CREDENTIAL_LENGTH);

    // Save current field before switching
    printf("[SetCredentials] Saving current field before switch...\n");
    saveFieldToEEPROM(currentField);

    // Switch field
    currentField = (currentField == CredentialField::WIFI_NAME) ?
                   CredentialField::WIFI_PASSWD : CredentialField::WIFI_NAME;

    // Reset character index for new field
    currentCharIndex = 0;

    printf("[SetCredentials] Switched to field: %s (current value: '%s', len=%zu/%d)\n",
           getCurrentFieldName(), buffers[static_cast<int>(currentField)].c_str(),
           buffers[static_cast<int>(currentField)].size(), MAX_CREDENTIAL_LENGTH);
    printf("[SetCredentials] ===== FIELD SWITCH COMPLETE =====\n\n");
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
    printf("\n[SetCredentials] ===== SAVING ALL FIELDS TO EEPROM =====\n");
    printf("[SetCredentials] Current SSID: '%s' (len=%zu/%d)\n",
           buffers[0].c_str(), buffers[0].size(), MAX_CREDENTIAL_LENGTH);
    printf("[SetCredentials] Current Password: '%s' (len=%zu/%d)\n",
           buffers[1].c_str(), buffers[1].size(), MAX_CREDENTIAL_LENGTH);

    saveFieldToEEPROM(CredentialField::WIFI_NAME);
    sleep_ms(20);  // Give EEPROM time between writes
    saveFieldToEEPROM(CredentialField::WIFI_PASSWD);
    sleep_ms(20);  // Final delay to ensure completion

    printf("[SetCredentials] ===== ALL FIELDS SAVED =====\n\n");
}