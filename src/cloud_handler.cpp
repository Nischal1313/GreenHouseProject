#include <string>
#include <cstring>
#include <cstdio>
#include "cloud_handler.h"
#include "sensor_handler.h"
#include "setCredentials.h"

CloudClass::CloudClass(
  const std::shared_ptr<SensorHandler> &sensorHandler,
  const SemaphoreHandle_t eepromMutex, Eeprom &eeprom)
  : resources(sensorHandler), eeprom(&eeprom),
    eepromMutex(eepromMutex) {
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
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

  // SSID
  if (eeprom->readBlock(EEPROM_WIFI_NAME_ADDR, tmp,
                        FIELD_SIZE)) {
    tmp[FIELD_SIZE - 1] = '\0';
    strncpy(ssid, reinterpret_cast<char *>(tmp), sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
  }

  // Password
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

void CloudClass::sendData(const int co2, const int temperature,
                          const int humidity, const int fanSpeed,
                          const int co2Setpoint) {
  char request[512];

  // Build POST request with all 5 fields and TalkBack key
  snprintf(request, sizeof(request),
           "POST /update.json HTTP/1.1\r\n"
           "Host: api.thingspeak.com\r\n"
           "User-Agent: PicoW\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Connection: close\r\n"
           "Content-Length: %d\r\n"
           "\r\n"
           "api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d&talkback_key=%s",
           0, // Content-Length placeholder
           THINGSPEAK_WRITE_API_KEY,
           co2, // field1: CO2 level (ppm)
           humidity, // field2: Relative humidity
           temperature, // field3: Temperature
           fanSpeed, // field4: Fan speed (0-100%)
           co2Setpoint, // field5: CO2 setpoint (ppm)
           TALKBACK_API_KEY
  );

  // Calculate actual content length
  char body[256];
  snprintf(body, sizeof(body),
           "api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d&talkback_key=%s",
           THINGSPEAK_WRITE_API_KEY, co2, humidity, temperature,
           fanSpeed, co2Setpoint, TALKBACK_API_KEY
  );
  const int content_length = strlen(body);

  // Rebuild with correct content length
  snprintf(request, sizeof(request),
           "POST /update.json HTTP/1.1\r\n"
           "Host: api.thingspeak.com\r\n"
           "User-Agent: PicoW\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Connection: close\r\n"
           "Content-Length: %d\r\n"
           "\r\n"
           "%s",
           content_length, body
  );

  printf(
    "Sending data to ThingSpeak: CO2=%d, Temp=%d, RH=%d, Fan=%d%%, Setpoint=%d\n",
    co2, temperature, humidity, fanSpeed, co2Setpoint);

  memset(tls_client_response, 0, sizeof(tls_client_response));

  const bool success = run_tls_client_test(
    reinterpret_cast<const uint8_t *>(root_ca),
    strlen(root_ca) + 1,
    TLS_CLIENT_SERVER,
    request,
    TLS_CLIENT_TIMEOUT_SECS
  );

  if (success) {
    printf("Data sent successfully to ThingSpeak\n");
  } else {
    printf("Failed to send data to ThingSpeak\n");
  }
}

int CloudClass::parseTalkBackCommand() {
  // Look for the command_string field in JSON response
  const char *cmd_start = strstr(tls_client_response,
                                 "\"command_string\":\"");
  if (!cmd_start) {
    return -1; // No command found
  }

  cmd_start += strlen("\"command_string\":\"");

  // Extract command string
  char cmd[64];
  int i = 0;
  while (*cmd_start && *cmd_start != '"' && i < 63) {
    cmd[i++] = *cmd_start++;
  }
  cmd[i] = '\0';

  printf("TalkBack command received: %s\n", cmd);

  // Parse SETPOINT=<value> command
  if (strncmp(cmd, "SETPOINT=", 9) == 0) {
    int value = atoi(cmd + 9);
    if (value >= MIN_CO2_SETPOINT && value <= MAX_CO2_SETPOINT) {
      return value;
    } else {
      printf("CO2 setpoint %d out of valid range (%d-%d)\n",
             value, MIN_CO2_SETPOINT, MAX_CO2_SETPOINT);
    }
  }

  return -1; // Invalid or out of range
}

void CloudClass::checkTalkBackQueue() {
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
    // call member function on this object
    this->storeSetpointToEEPROM(newSetPoint);
  } else {
    printf("[Cloud] No valid TalkBack setpoint found (%d)\n",
           newSetPoint);
  }
}


void CloudClass::taskEntry(void *pvParameters) {
  auto *self = static_cast<CloudClass *>(pvParameters);
  self->Cloudtask();
}

[[noreturn]] void CloudClass::Cloudtask() {
  printf("cloud task ran");
  bool network_connected = false;

  printf("CloudTask started\n");

  while (true) {
    if (ssid[0] == '\0') {
      loadCredentialsFromEEPROM();
      if (ssid[0] == '\0') {
        printf("Waiting for WiFi credentials in EEPROM...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }


    if (!network_connected) {
      printf("Connecting to WiFi...\n");
      connect();
      network_connected = true;
      transmit = true;
    }

    // Transmit data if connected
    if (transmit) {
      // Read current sensor values with mutex protection
      int co2_value, temp_value, humidity_value, fan_speed_value,
          setpoint_value; {
        // Get all sensor readings in one call
        const SensorValues readings = resources->getReadings();

        // Extract individual values and convert to int
        co2_value = static_cast<int>(readings.co2);
        temp_value = static_cast<int>(readings.temperature);
        humidity_value = static_cast<int>(readings.humidity);
        fan_speed_value = static_cast<int>(readings.fanSpeed);
        setpoint_value = static_cast<int>(readings.targetCo2);
      }

      // Send data to ThingSpeak and check for new commands
      sendData(co2_value, temp_value, humidity_value, fan_speed_value,
               setpoint_value);

      // Wait a bit before checking TalkBack (ThingSpeak rate limit)
      vTaskDelay(pdMS_TO_TICKS(5000));

      // Check for new commands from TalkBack
      checkTalkBackQueue();
    }
    // Wait before next update cycle
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY));
  }
}
