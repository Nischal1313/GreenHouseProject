
// setCredentials.h - Updated header with clearCurrentField
#ifndef SET_CREDENTIALS_H
#define SET_CREDENTIALS_H

#include <string>
#include "eeprom.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "hardware/gpio.h"

enum class CredentialField {
    WIFI_NAME,
    WIFI_PASSWD
};

enum class CharsetMode {
    LOWERCASE,
    UPPERCASE,
    NUMBERS
};

class SetCredentials {
public:
    explicit SetCredentials(Eeprom& eeprom, SemaphoreHandle_t eepromMutex);

    void setChars();

    // Field control
    void nextField();
    const char* getCurrentFieldName() const;
    CredentialField getCurrentField() const { return currentField; }

    // Charset control
    void nextCharset();
    CharsetMode getCharsetMode() const;

    // Character editing
    char getCurrentChar() const;
    void rotateChar(int direction);
    void confirmChar();
    void clearCurrentField();  // Added method to clear current field

    // Buffers
    const char* getCurrentBuffer();
    void saveAllToEEPROM();

    // Retrieval
    const char* getWifiSSID() const { return buffers[0].c_str(); }
    const char* getWifiPassword() const { return buffers[1].c_str(); }

private:
    static constexpr uint16_t EEPROM_WIFI_NAME_ADDR = 0x0100;
    static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR = 0x0140;
    static constexpr uint16_t FIELD_SIZE = 32;  // Increased to 32 chars for WiFi credentials
    static constexpr uint ENCODER_BUTTON_PIN = 12;

    Eeprom& eeprom;
    SemaphoreHandle_t eepromMutex;
    CredentialField currentField;
    CharsetMode charsetMode;
    size_t currentCharIndex;
    std::string buffers[2];
    std::string charsets[3];

    std::string& currentBuffer();
    const std::string& currentBuffer() const;
    void loadFromEEPROM();
    void saveFieldToEEPROM(CredentialField field);
    bool isValidString(const uint8_t* data, size_t len) const;
    bool lastButtonState = true;
    absolute_time_t lastButtonTime;
    bool debounceButtonPressed();
};

#endif