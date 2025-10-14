#ifndef CLOUDTASK_H
#define CLOUDTASK_H

#include <mbedtls/debug.h>
#include <memory>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "sensor_handler.h"
#include "eeprom/eeprom.h"
#include "apiKey.h"
#include "ipstack/IPStack.h"
#include "ipstack/lwipopts.h"
#include "ipstack/lwipopts_tls.h"
#include "ipstack/mbedtls_config.h"

#define TLS_CLIENT_SERVER "api.thingspeak.com"
#define TLS_CLIENT_TIMEOUT_SECS 15

// ThingSpeak API keys
class CloudClass {
public:
  CloudClass(const std::shared_ptr<SensorHandler> &sensorHandler,
             SemaphoreHandle_t eepromMutex, Eeprom &eeprom);

  void storeSetpointToEEPROM(int setpoint) const;

  void connect();

  void setCredentials(const char *ssid, const char *password);

  void loadCredentialsFromEEPROM();

  static void sendData(int co2, int temperature, int humidity,
                       int fanSpeed, int co2Setpoint);

  void checkTalkBackQueue();

  static void taskEntry(void *pvParameters);

  [[noreturn]] void Cloudtask();

  bool transmit = false;

private:
  static constexpr uint16_t EEPROM_WIFI_NAME_ADDR = 0x0500;
  static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR = 0x0600;
  static constexpr uint16_t FIELD_SIZE = 32;
  static constexpr uint TASK_DELAY = 60000;
  // 60 seconds between updates
  static constexpr int MIN_CO2_SETPOINT = 200;
  static constexpr int MAX_CO2_SETPOINT = 15000;
  static constexpr uint16_t EEPROM_CO2_CLOUD_ADDR = 0x0200;

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

extern char tls_client_response[2048];
extern const char root_ca[];
}

#endif // CLOUDTASK_H
