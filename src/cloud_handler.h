#pragma once
#include <string>
#include <functional>
#include "sensor_handler.h"
#include "setpoint_manager.h"
#include "setCredentials.h"
#include "mutexGuard.h"
#include "FreeRTOS.h"
#include "task.h"

// Forward declare the C TLS functions
extern "C" {
  bool run_tls_client_test(const uint8_t *cert, size_t cert_len,
                           const char *server, const char *request, int timeout);
  int get_co2_setpoint(void);
  const char* get_last_tls_response(void);
}

class CloudHandler {
public:
  CloudHandler(SensorHandler* sensorHandler,
               SetpointManager* setpointManager,
               SetCredentials* credentials);

  // Start the cloud task
  void startTask(uint32_t intervalMs = 15000);

  // FreeRTOS task entry
  static void taskEntry(void* pvParameters);

  // Set response callback to parse TLS responses
  void setResponseCallback(std::function<void(const char*)> callback);

private:
  SensorHandler* sensorHandler;
  SetpointManager* setpointManager;
  SetCredentials* credentials;

  uint32_t updateIntervalMs;

  // API Keys - loaded from credentials/EEPROM
  std::string writeApiKey;
  std::string talkbackApiKey;
  std::string talkbackId;

  // Response parsing
  std::function<void(const char*)> responseCallback;
  static char lastResponse[2048];
  static int lastSetpoint;

  // Internal methods
  [[noreturn]] void cloudTask();
  bool connectWiFi();
  bool sendSensorData();
  bool fetchCloudSetpoint();

  // Parse ThingSpeak TalkBack response
  static int parseSetpointFromResponse(const char* response);

  // Build HTTP requests
  std::string buildUpdateRequest(float co2, float temp, float hum,
                                 float fan, int setpoint);
  std::string buildTalkBackRequest();
};
