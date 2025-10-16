#include <string>
#include <cstring>
#include <cstdio>
#include "cloud_handler.h"
#include "sensor_handler.h"
#include "setCredentials.h"

SemaphoreHandle_t CloudClass::tlsMutex = nullptr;

CloudClass::CloudClass(
  const std::shared_ptr<SensorHandler> &sensorHandler,
  const SemaphoreHandle_t eepromMutex, Eeprom &eeprom)
  : resources(sensorHandler), eeprom(&eeprom),
    eepromMutex(eepromMutex) {
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));

  if (tlsMutex == nullptr) {
    tlsMutex = xSemaphoreCreateMutex();
    if (tlsMutex == nullptr) {
      printf("[Cloud] FATAL: Failed to create TLS mutex\n");
    }
  }
}


void CloudClass::storeSetpointToEEPROM(const int setpoint) const {
  const MutexGuard lock(eepromMutex);
  const uint8_t buf[2] = {
    static_cast<uint8_t>(setpoint >> 8),
    static_cast<uint8_t>(setpoint & 0xFF)
  };
  eeprom->writeBlock(EEPROM_CO2_CLOUD_ADDR, buf, 2);
  printf("[Cloud] Stored new CO2 setpoint %d to EEPROM\n", setpoint);
}


void CloudClass::setCredentials(const char *ssid,
                                const char *password) {
  strncpy(this->ssid, ssid, sizeof(this->ssid) - 1);
  strncpy(this->password, password, sizeof(this->password) - 1);
  this->ssid[sizeof(this->ssid) - 1] = '\0';
  this->password[sizeof(this->password) - 1] = '\0';
}

void CloudClass::loadCredentialsFromEEPROM() {
  uint8_t tmp[FIELD_SIZE];
  const MutexGuard lock(eepromMutex);

  if (!lock.owns_lock()) {
    printf("[Cloud] Failed to acquire EEPROM mutex\n");
    return;
  }

  if (eeprom->readBlock(EEPROM_WIFI_NAME_ADDR, tmp,
                        FIELD_SIZE)) {
    tmp[FIELD_SIZE - 1] = '\0';
    strncpy(ssid, reinterpret_cast<char *>(tmp), sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
  }


  if (eeprom->readBlock(EEPROM_WIFI_PASSWD_ADDR, tmp,
                        FIELD_SIZE)) {
    tmp[FIELD_SIZE - 1] = '\0';
    strncpy(password, reinterpret_cast<char *>(tmp),
            sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
  }

  printf("[Cloud] Loaded credentials from EEPROM: SSID=%s\n", ssid);
}


void CloudClass::connect() {
  if (cyw43_arch_init()) {
    printf("Failed to initialize WiFi\n");
    return;
  }

  cyw43_arch_enable_sta_mode();

  printf("Connecting to WiFi: %s\n", ssid);
  if (cyw43_arch_wifi_connect_timeout_ms(ssid, password,
                                         CYW43_AUTH_WPA2_AES_PSK,
                                         30000)) {
    printf("Failed to connect to WiFi\n");
    return;
  }

  printf("WiFi connected successfully\n");
}


bool CloudClass::sendData(const int co2, const int temperature,
                          const int humidity, const int fanSpeed,
                          const int co2Setpoint) {

  const MutexGuard lock(tlsMutex);
  if (!lock.owns_lock()) {
    printf("[Cloud] Failed to acquire TLS mutex for sendData\n");
    return false;
  }

  char body[256];
  snprintf(body, sizeof(body),
           "api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d&talkback_key=%s",
           THINGSPEAK_WRITE_API_KEY,
           co2, humidity, temperature, fanSpeed, co2Setpoint,
           TALKBACK_API_KEY);

  const int content_length = strlen(body);

  char request[512];
  snprintf(request, sizeof(request),
           "POST /update.json HTTP/1.1\r\n"
           "Host: api.thingspeak.com\r\n"
           "User-Agent: PicoW\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Connection: close\r\n"
           "Content-Length: %d\r\n"
           "\r\n"
           "%s",
           content_length, body);

  printf("[Cloud] Sending data to ThingSpeak...\n");

  memset(tls_client_response, 0, sizeof(tls_client_response));


  bool success = run_tls_client_test(
    reinterpret_cast<const uint8_t *>(root_ca),
    strlen(root_ca) + 1,
    "api.thingspeak.com",
    request,
    TLS_CLIENT_TIMEOUT_SECS);

  if (!success) {
    printf("[Cloud] Failed to send data to ThingSpeak.\n");
    return false;
  }


  if (strstr(tls_client_response, "HTTP/1.1 200 OK") ||
      strstr(tls_client_response, "HTTP/1.0 200 OK")) {
    printf("[Cloud] Data sent successfully to ThingSpeak.\n");
    return true;
  }

  printf("[Cloud] ThingSpeak response did not confirm success.\n");
  printf("[Cloud] Response: %s\n", tls_client_response);
  return false;
}

int CloudClass::parseTalkBackCommand() {

  const char *cmd_start = strstr(tls_client_response,
                                 "\"command_string\":\"");
  if (!cmd_start) {
    return -1;
  }

  cmd_start += strlen("\"command_string\":\"");


  char cmd[64];
  int i = 0;
  while (*cmd_start && *cmd_start != '"' && i < 63) {
    cmd[i++] = *cmd_start++;
  }
  cmd[i] = '\0';

  printf("TalkBack command received: %s\n", cmd);

  if (strncmp(cmd, "SETPOINT=", 9) == 0) {
    int value = atoi(cmd + 9);
    if (value >= MIN_CO2_SETPOINT && value <= MAX_CO2_SETPOINT) {
      return value;
    }
    printf("CO2 setpoint %d out of valid range (%d-%d)\n",
           value, MIN_CO2_SETPOINT, MAX_CO2_SETPOINT);
  }

  return -1;
}

void CloudClass::checkTalkBackQueue() const {

  const MutexGuard lock(tlsMutex);
  if (!lock.owns_lock()) {
    printf(
      "[Cloud] Failed to acquire TLS mutex for checkTalkBackQueue\n");
    return;
  }

  char request[256];

  snprintf(request, sizeof(request),
           "GET /talkbacks/%s/commands/execute.json?api_key=%s HTTP/1.1\r\n"
           "Host: api.thingspeak.com\r\n"
           "User-Agent: PicoW\r\n"
           "Connection: close\r\n\r\n",
           TALKBACK_ID, TALKBACK_API_KEY);

  memset(tls_client_response, 0, sizeof(tls_client_response));

  bool success = run_tls_client_test(
    reinterpret_cast<const uint8_t *>(root_ca),
    strlen(root_ca) + 1,
    TLS_CLIENT_SERVER,
    request,
    TLS_CLIENT_TIMEOUT_SECS
  );

  if (!success) {
    printf(
      "[Cloud] TalkBack request failed, will retry next cycle.\n");
    return;
  }

  const int newSetPoint = parseTalkBackCommand();

  if (newSetPoint <= MAX_CO2_SETPOINT && newSetPoint >=
      MIN_CO2_SETPOINT) {
    this->storeSetpointToEEPROM(newSetPoint);
  }
}

void CloudClass::dataSendTaskEntry(void *pvParameters) {
  auto *self = static_cast<CloudClass *>(pvParameters);
  self->dataSendTask();
}

[[noreturn]] void CloudClass::dataSendTask() {
  printf("[DataSend] Task started\n");

  while (true) {
    if (!network_connected) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!transmit) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    int co2_value, temp_value, humidity_value, fan_speed_value,
        setpoint_value; {
      const SensorValues readings = resources->getReadings();

      // Extract individual values and convert to int
      co2_value = static_cast<int>(readings.co2);
      temp_value = static_cast<int>(readings.temperature);
      humidity_value = static_cast<int>(readings.humidity);
      fan_speed_value = static_cast<int>(readings.fanSpeed);
      setpoint_value = static_cast<int>(readings.targetCo2);
    }

    if (sendData(co2_value, temp_value, humidity_value,
                 fan_speed_value, setpoint_value)) {
      printf("\n");
      printf("[DataSend] Transmission acknowledged.\n");
      printf("\n");
    } else {
      printf("[DataSend] Transmission failed or unverified.\n");
    }

    vTaskDelay(pdMS_TO_TICKS(DATA_SEND_DELAY));
  }
}


void CloudClass::talkbackPollTaskEntry(void *pvParameters) {
  auto *self = static_cast<CloudClass *>(pvParameters);
  self->talkbackPollTask();
}

[[noreturn]] void CloudClass::talkbackPollTask() {
  printf("[TalkBack] Task started\n");


  while (true) {
    if (ssid[0] == '\0') {
      loadCredentialsFromEEPROM();
      if (ssid[0] == '\0') {
        printf(
          "[TalkBack] Waiting for WiFi credentials in EEPROM...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }

    if (network_connected) {
      break;
    }

    if (!network_connected) {
      printf("[TalkBack] Network not connected, connecting...\n");
      connect();
      network_connected = true;
      transmit = true;
    }
  }


  printf("[TalkBack] Starting command polling loop\n");
  while (true) {
    checkTalkBackQueue();

    vTaskDelay(pdMS_TO_TICKS(TALKBACK_POLL_DELAY));
  }
}
