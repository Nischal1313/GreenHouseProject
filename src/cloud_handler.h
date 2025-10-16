#pragma once
#include <memory>
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "sensor_handler.h"
#include "eeprom/eeprom.h"
#include "apiKey.h"
#include "mutexGuard.h"


// #define TLS_CLIENT_SERVER "api.thingspeak.com"

class CloudClass {
public:
  CloudClass(const std::shared_ptr<SensorHandler> &sensorHandler,
             SemaphoreHandle_t eepromMutex, Eeprom &eeprom);

  void storeSetpointToEEPROM(int setpoint) const;

  void connect();

  void setCredentials(const char *ssid, const char *password);

  void loadCredentialsFromEEPROM();

  bool sendData(int co2, int temperature, int humidity,
                int fanSpeed, int co2Setpoint);

  void checkTalkBackQueue() const;

  static void dataSendTaskEntry(void *pvParameters);
  static void talkbackPollTaskEntry(void *pvParameters);

  [[noreturn]] void dataSendTask();
  [[noreturn]] void talkbackPollTask();

  bool transmit = false;
  bool network_connected = false;

  // Mutex to protect TLS operations (shared between both tasks)
  static SemaphoreHandle_t tlsMutex;

private:
  static constexpr uint16_t EEPROM_WIFI_NAME_ADDR = 0x0500;
  static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR = 0x0600;
  static constexpr uint16_t FIELD_SIZE = 32;
  static constexpr uint DATA_SEND_DELAY = 60000;  // Send data every 60 seconds
  static constexpr uint TALKBACK_POLL_DELAY = 5000;  // Poll TalkBack every 5 seconds
  static constexpr int MIN_CO2_SETPOINT = 200;
  static constexpr int MAX_CO2_SETPOINT = 15000;
  static constexpr uint16_t EEPROM_CO2_CLOUD_ADDR = 0x0200;
  static constexpr uint8_t TLS_CLIENT_TIMEOUT_SECS = 15;
  static constexpr const char *TLS_CLIENT_SERVER = &*
      "api.thingspeak.com";

  char ssid[32]{};
  char password[64]{};
  std::shared_ptr<SensorHandler> resources;
  Eeprom *eeprom;
  SemaphoreHandle_t eepromMutex;

  static int parseTalkBackCommand();
};


extern "C" {
bool run_tls_client_test(const uint8_t *cert, size_t cert_len,
                         const char *server, const char *request,
                         int timeout);

extern char tls_client_response[2048*2];
extern const char root_ca[];
}
