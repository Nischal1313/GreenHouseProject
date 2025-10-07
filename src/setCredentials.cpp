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

    charsets[0] = "abcdefghijklmnopqrstuvwxyz";
    charsets[1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    charsets[2] = "0123456789";

    buffers[0] = "EMPTY";
    buffers[1] = "EMPTY";

    loadFromEEPROM();
    setChars();

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

    if (pressed && absolute_time_diff_us(lastButtonTime, now) > DEBOUNCE_MS * 1000) {
        lastButtonTime = now;
        return true;
    }
    return false;
}

void SetCredentials::loadFromEEPROM() {
    uint8_t tmp[FIELD_SIZE];
    MutexGuard lock(eepromMutex);

    for (int idx = 0; idx < 2; ++idx) {
        memset(tmp, 0, sizeof(tmp));
        uint16_t addr = (idx == 0) ? EEPROM_WIFI_NAME_ADDR : EEPROM_WIFI_PASSWD_ADDR;

        if (lock.owns_lock() && eeprom.readBlock(addr, tmp, FIELD_SIZE)) {
            tmp[FIELD_SIZE - 1] = '\0';
            buffers[idx] = reinterpret_cast<char*>(tmp);
            printf("[EEPROM] Loaded buffer %d: '%s'\n", idx, buffers[idx].c_str());
        }

        if (buffers[idx].empty()) buffers[idx] = "EMPTY";
    }
}

char SetCredentials::getCurrentChar() const {
    const std::string& buf = currentBuffer();
    if (buf.empty() || currentCharIndex >= buf.size()) return ' ';
    return buf[currentCharIndex];
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
    uint16_t offset = (field == CredentialField::WIFI_NAME) ? EEPROM_WIFI_NAME_ADDR : EEPROM_WIFI_PASSWD_ADDR;
    int idx = static_cast<int>(field);

    MutexGuard lock(eepromMutex);
    if (lock.owns_lock()) {
        const auto& buf = buffers[idx];
        size_t writeLen = std::min(buf.size() + 1, static_cast<size_t>(FIELD_SIZE));
        eeprom.writeBlock(offset, reinterpret_cast<const uint8_t*>(buf.c_str()), writeLen);
        printf("[EEPROM] Saved field %d, len=%zu, data='%s'\n", idx, writeLen, buf.c_str());
    }
}


void SetCredentials::rotateChar(int direction) {
    const auto& cs = charsets[static_cast<int>(charsetMode)];
    if (cs.empty()) return;
    int len = static_cast<int>(cs.size());
    currentCharIndex = (currentCharIndex + direction + len) % len;
    printf("[SetCredentials] Rotated char to '%c' (index=%zu)\n", cs[currentCharIndex], currentCharIndex);
}

void SetCredentials::confirmChar() {
    auto& buf = currentBuffer();
    if (buf == "EMPTY") buf.clear();

    if (buf.size() < FIELD_SIZE - 2) {
        char c = getCurrentChar();
        buf.push_back(c);
        printf("[SetCredentials] Confirmed '%c', buffer='%s'\n", c, buf.c_str());
    }
}

const char* SetCredentials::getCurrentBuffer() {
    if (debounceButtonPressed()) {
        printf("[SetCredentials] Encoder button pressed — confirming char.\n");
        confirmChar();
    }

    const auto& ref = buffers[static_cast<int>(currentField)];
    if (ref.empty()) return "EMPTY";
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
    saveFieldToEEPROM(currentField);
    currentField = (currentField == CredentialField::WIFI_NAME) ?
                   CredentialField::WIFI_PASSWD : CredentialField::WIFI_NAME;
    currentCharIndex = 0;
    printf("[SetCredentials] Switched to field: %s\n", getCurrentFieldName());
}

void SetCredentials::nextCharset() {
    int idx = static_cast<int>(charsetMode);
    charsetMode = static_cast<CharsetMode>((idx + 1) % 3);
    currentCharIndex = 0;

    const char* modeName =
        (charsetMode == CharsetMode::LOWERCASE) ? "LOWERCASE" :
        (charsetMode == CharsetMode::UPPERCASE) ? "UPPERCASE" :
        "NUMBERS";
    printf("[SetCredentials] Charset changed to: %s\n", modeName);
}
