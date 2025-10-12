#include "cloud_handler.h"
#include "pico/cyw43_arch.h"
#include <cstdio>
#include <cstring>
#include <sstream>

// Static members for response handling
char CloudHandler::lastResponse[2048] = {0};
int CloudHandler::lastSetpoint = -1;

CloudHandler::CloudHandler(SensorHandler* sensorHandler,
                           SetpointManager* setpointManager,
                           SetCredentials* credentials)
    : sensorHandler(sensorHandler),
      setpointManager(setpointManager),
      credentials(credentials),
      updateIntervalMs(15000),
      writeApiKey("1WWH2NWXSM53URR5"),      // Your write API key
      talkbackApiKey("371DAWENQKI6J8DD"),   // Your talkback API key
      talkbackId("52920")                    // Your talkback ID
{
    // Could load API keys from EEPROM via credentials if needed
}

void CloudHandler::setResponseCallback(std::function<void(const char*)> callback) {
    responseCallback = callback;
}

bool CloudHandler::connectWiFi() {
    // Check if already initialized
    static bool initialized = false;

    if (!initialized) {
        if (cyw43_arch_init()) {
            printf("[CloudHandler] Failed to init CYW43\n");
            return false;
        }
        cyw43_arch_enable_sta_mode();
        initialized = true;
    }

    // Get credentials
    const char* ssid = credentials->getWifiSSID();
    const char* password = credentials->getWifiPassword();

    printf("[CloudHandler] Connecting to '%s'...\n", ssid);

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("[CloudHandler] Failed to connect to WiFi\n");
        return false;
    }

    printf("[CloudHandler] WiFi connected!\n");
    return true;
}

std::string CloudHandler::buildUpdateRequest(float co2, float temp, float hum,
                                             float fan, int setpoint) {
    std::ostringstream oss;

    // Build POST request to update ThingSpeak fields AND execute TalkBack
    oss << "POST /update.json HTTP/1.1\r\n"
        << "Host: api.thingspeak.com\r\n"
        << "Content-Type: application/x-www-form-urlencoded\r\n"
        << "Content-Length: ";

    // Build body first to calculate content length
    std::ostringstream body;
    body << "field1=" << static_cast<int>(co2)
         << "&field2=" << static_cast<int>(temp)
         << "&field3=" << static_cast<int>(hum)
         << "&field4=" << static_cast<int>(fan)
         << "&field5=" << setpoint
         << "&api_key=" << writeApiKey
         << "&talkback_key=" << talkbackApiKey;

    std::string bodyStr = body.str();

    oss << bodyStr.length() << "\r\n"
        << "\r\n"
        << bodyStr;

    return oss.str();
}

std::string CloudHandler::buildTalkBackRequest() {
    std::ostringstream oss;

    // Execute (get and remove) next command from TalkBack queue
    oss << "POST /talkbacks/" << talkbackId << "/commands/execute.json HTTP/1.1\r\n"
        << "Host: api.thingspeak.com\r\n"
        << "Content-Type: application/x-www-form-urlencoded\r\n";

    std::string body = "api_key=" + talkbackApiKey;

    oss << "Content-Length: " << body.length() << "\r\n"
        << "\r\n"
        << body;

    return oss.str();
}

int CloudHandler::parseSetpointFromResponse(const char* response) {
    if (!response) return -1;

    // Look for "command_string" field in JSON response
    // Example: {"command_string":"SETPOINT=850", ...}
    const char* cmdStr = strstr(response, "\"command_string\"");
    if (!cmdStr) {
        printf("[CloudHandler] No command_string found in response\n");
        return -1;
    }

    // Look for SETPOINT= pattern
    const char* setpointStr = strstr(cmdStr, "SETPOINT=");
    if (!setpointStr) {
        printf("[CloudHandler] No SETPOINT= found in command\n");
        return -1;
    }

    // Parse the number after SETPOINT=
    int value = 0;
    if (sscanf(setpointStr, "SETPOINT=%d", &value) == 1) {
        if (value >= 400 && value <= 2000) {
            printf("[CloudHandler] Parsed setpoint: %d ppm\n", value);
            return value;
        } else {
            printf("[CloudHandler] Setpoint out of range: %d\n", value);
        }
    }

    return -1;
}

bool CloudHandler::sendSensorData() {
    // Get latest sensor readings
    SensorValues readings = sensorHandler->getLatestReadings();
    int currentSetpoint = setpointManager->getEffectiveTarget();

    printf("[CloudHandler] Sending: CO2=%.0f, Temp=%.1f, Hum=%.1f, Fan=%.0f, SP=%d\n",
           readings.co2, readings.temperature, readings.humidity,
           readings.fanSpeed, currentSetpoint);

    // Build request
    std::string request = buildUpdateRequest(
        readings.co2,
        readings.temperature,
        readings.humidity,
        readings.fanSpeed,
        currentSetpoint
    );

    // Send via TLS
    bool success = run_tls_client_test(
        nullptr,                        // No certificate (VERIFY_OPTIONAL)
        0,
        "api.thingspeak.com",
        request.c_str(),
        15                              // 15 second timeout
    );

    if (success) {
        printf("[CloudHandler] Data sent successfully\n");
    } else {
        printf("[CloudHandler] Failed to send data\n");
    }

    return success;
}

bool CloudHandler::fetchCloudSetpoint() {
    printf("[CloudHandler] Fetching cloud setpoint...\n");

    // Build TalkBack request
    std::string request = buildTalkBackRequest();

    // Send via TLS
    bool success = run_tls_client_test(
        nullptr,
        0,
        "api.thingspeak.com",
        request.c_str(),
        15
    );

    if (!success) {
        printf("[CloudHandler] Failed to fetch setpoint\n");
        return false;
    }

    // Get parsed setpoint from response handler
    int newSetpoint = get_co2_setpoint();

    if (newSetpoint > 0) {
        printf("[CloudHandler] Cloud setpoint received: %d ppm\n", newSetpoint);
        setpointManager->updateCloud(newSetpoint);
        return true;
    } else {
        printf("[CloudHandler] No new setpoint in cloud response\n");
        return false;
    }
}

[[noreturn]] void CloudHandler::cloudTask() {
    printf("[CloudHandler] Task started\n");

    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (true) {
        // Connect to WiFi
        if (!connectWiFi()) {
            printf("[CloudHandler] WiFi connection failed, retrying in 30s\n");
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }

        // Send sensor data to cloud
        sendSensorData();

        // Small delay between requests
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Fetch setpoint from cloud
        fetchCloudSetpoint();

        // Wait before next update cycle
        printf("[CloudHandler] Sleeping for %lu ms\n", updateIntervalMs);
        vTaskDelay(pdMS_TO_TICKS(updateIntervalMs));
    }
}

void CloudHandler::taskEntry(void* pvParameters) {
    auto* self = static_cast<CloudHandler*>(pvParameters);
    self->cloudTask();
}

void CloudHandler::startTask(uint32_t intervalMs) {
    updateIntervalMs = intervalMs;

    xTaskCreate(
        taskEntry,
        "CloudHandler",
        4096,                // Stack size
        this,
        1,                   // Priority (low)
        nullptr
    );

    printf("[CloudHandler] Task created with %lu ms interval\n", intervalMs);
}
