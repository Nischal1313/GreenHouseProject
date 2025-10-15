#pragma once

#include <string>
#include "eeprom/eeprom.h"
#include "FreeRTOS.h"
#include "semphr.h"

enum class CredentialField {
  WIFI_NAME,
  WIFI_PASSWD
};

enum class CharsetMode {
  LOWERCASE,
  UPPERCASE,
  NUMBERS
};

class CredentialValidator {
public:
  static bool isValidString(const uint8_t *data, size_t len,
                            size_t fieldSize);
};

class SetCredentials {
public:
  explicit SetCredentials(Eeprom &eeprom,
                          SemaphoreHandle_t eepromMutex);

  void nextField();

  [[nodiscard]] const char *getCurrentFieldName() const;

  [[nodiscard]] CredentialField getCurrentField() const {
    return currentField;
  }

  void nextCharset();

  [[nodiscard]] CharsetMode getCharsetMode() const;

  [[nodiscard]] char getCurrentChar() const;

  void rotateChar(int direction);

  void confirmChar();

  void clearCurrentField();

  const char *getCurrentBuffer();

  void saveAllToEEPROM() const;

  [[nodiscard]] const char *getWifiSSID() const {
    return buffers[0].c_str();
  }

  [[nodiscard]] const char *getWifiPassword() const {
    return buffers[1].c_str();
  }

private:
  static constexpr uint16_t EEPROM_WIFI_NAME_ADDR = 0x0500;
  static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR = 0x0600;
  static constexpr uint16_t FIELD_SIZE = 32;
  static constexpr uint MAX_CREDENTIAL_LENGTH = 15;
  static constexpr uint EEPROM_SLEEP_MS = 20;

  Eeprom &eeprom;
  SemaphoreHandle_t eepromMutex;
  CredentialField currentField;
  CharsetMode charsetMode;
  size_t currentCharIndex;
  std::string buffers[2];
  std::string charsets[3];

  void loadFromEEPROM();

  void saveFieldToEEPROM(CredentialField field) const;

  std::string &currentBuffer();

  [[nodiscard]] const std::string &currentBuffer() const;
};
