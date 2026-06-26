#include <string>
#include <cstring>
#include <cstdio>
#include "cloud_handler.h"
#include "sensor_handler.h"
#include "setCredentials.h"

SemaphoreHandle_t CloudClass::tlsMutexS{nullptr};

CloudClass::CloudClass(
    std::shared_ptr<SensorHandler> const &pSensorHandlerP,
    SemaphoreHandle_t const eepromMutexP,
    Eeprom &rEepromP)
    : pResourcesM{pSensorHandlerP},
      pEepromM{&rEepromP},
      eepromMutexM{eepromMutexP}
{
    memset(ssidM, 0, sizeof(ssidM));
    memset(passwordM, 0, sizeof(passwordM));

    if (tlsMutexS == nullptr)
    {
        tlsMutexS = xSemaphoreCreateMutex();
        if (tlsMutexS == nullptr)
        {
            printf("[Cloud] FATAL: Failed to create TLS mutex\n");
        }
    }
}


void CloudClass::storeSetpointToEEPROM(int const setpointP) const
{
    const MutexGuard lock(eepromMutexM);
    uint8_t const buf[2]{
        static_cast<uint8_t>(setpointP >> 8),
        static_cast<uint8_t>(setpointP & 0xFF)
    };
    pEepromM->writeBlock(EEPROM_CO2_CLOUD_ADDR, buf, 2);
    printf("[Cloud] Stored new CO2 setpoint %d to EEPROM\n", setpointP);
}


void CloudClass::setCredentials(char const *pSsidP,
                                char const *pPasswordP)
{
    strncpy(ssidM, pSsidP, sizeof(ssidM) - 1);
    strncpy(passwordM, pPasswordP, sizeof(passwordM) - 1);
    ssidM[sizeof(ssidM) - 1] = '\0';
    passwordM[sizeof(passwordM) - 1] = '\0';
}


void CloudClass::loadCredentialsFromEEPROM()
{
    uint8_t tmp[FIELD_SIZE];
    const MutexGuard lock(eepromMutexM);

    if (lock.owns_lock())
    {
        if (pEepromM->readBlock(EEPROM_WIFI_NAME_ADDR, tmp,
                                FIELD_SIZE))
        {
            tmp[FIELD_SIZE - 1] = '\0';
            strncpy(ssidM, reinterpret_cast<char *>(tmp), sizeof(ssidM) - 1);
            ssidM[sizeof(ssidM) - 1] = '\0';
        }

        if (pEepromM->readBlock(EEPROM_WIFI_PASSWD_ADDR, tmp,
                                FIELD_SIZE))
        {
            tmp[FIELD_SIZE - 1] = '\0';
            strncpy(passwordM, reinterpret_cast<char *>(tmp),
                    sizeof(passwordM) - 1);
            passwordM[sizeof(passwordM) - 1] = '\0';
        }

        printf("[Cloud] Loaded credentials from EEPROM: SSID=%s\n", ssidM);
    }
    else
    {
        printf("[Cloud] Failed to acquire EEPROM mutex\n");
    }
}


void CloudClass::connect()
{
    if (cyw43_arch_init())
    {
        printf("Failed to initialize WiFi\n");
    }
    else
    {
        cyw43_arch_enable_sta_mode();

        printf("Connecting to WiFi: %s\n", ssidM);
        if (cyw43_arch_wifi_connect_timeout_ms(ssidM, passwordM,
                                                CYW43_AUTH_WPA2_AES_PSK,
                                                30000))
        {
            printf("Failed to connect to WiFi\n");
        }
        else
        {
            printf("WiFi connected successfully\n");
        }
    }
}


bool CloudClass::sendData(int const co2P, int const temperatureP,
                          int const humidityP, int const fanSpeedP,
                          int const co2SetpointP)
{
    bool result{false};

    const MutexGuard lock(tlsMutexS);
    if (lock.owns_lock())
    {
        char body[256];
        snprintf(body, sizeof(body),
                 "api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d&talkback_key=%s",
                 THINGSPEAK_WRITE_API_KEY,
                 co2P, humidityP, temperatureP, fanSpeedP, co2SetpointP,
                 TALKBACK_API_KEY);

        int const content_length{static_cast<int>(strlen(body))};

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

        memset(tlsClientResponseG, 0, sizeof(tlsClientResponseG));

        bool const success{run_tls_client_test(
            reinterpret_cast<uint8_t const *>(rootCaG),
            strlen(rootCaG) + 1,
            "api.thingspeak.com",
            request,
            TLS_CLIENT_TIMEOUT_SECS)};

        if (success)
        {
            if (strstr(tlsClientResponseG, "HTTP/1.1 200 OK") ||
                strstr(tlsClientResponseG, "HTTP/1.0 200 OK"))
            {
                printf("[Cloud] Data sent successfully to ThingSpeak.\n");
                result = true;
            }
            else
            {
                printf("[Cloud] ThingSpeak response did not confirm success.\n");
                printf("[Cloud] Response: %s\n", tlsClientResponseG);
            }
        }
        else
        {
            printf("[Cloud] Failed to send data to ThingSpeak.\n");
        }
    }
    else
    {
        printf("[Cloud] Failed to acquire TLS mutex for sendData\n");
    }

    return result;
}


int CloudClass::parseTalkBackCommand()
{
    int result{-1};

    char const *cmdStart{strstr(tlsClientResponseG,
                                "\"command_string\":\"")};
    if (cmdStart)
    {
        cmdStart += strlen("\"command_string\":\"");

        char cmd[64];
        int i{0};
        while (*cmdStart && *cmdStart != '"' && i < 63)
        {
            cmd[i++] = *cmdStart++;
        }
        cmd[i] = '\0';

        printf("TalkBack command received: %s\n", cmd);

        if (strncmp(cmd, "SETPOINT=", 9) == 0)
        {
            int const value{atoi(cmd + 9)};
            if (value >= MIN_CO2_SETPOINT && value <= MAX_CO2_SETPOINT)
            {
                result = value;
            }
            else
            {
                printf("CO2 setpoint %d out of valid range (%d-%d)\n",
                       value, MIN_CO2_SETPOINT, MAX_CO2_SETPOINT);
            }
        }
    }

    return result;
}


void CloudClass::checkTalkBackQueue() const
{
    const MutexGuard lock(tlsMutexS);
    if (lock.owns_lock())
    {
        char request[256];

        snprintf(request, sizeof(request),
                 "GET /talkbacks/%s/commands/execute.json?api_key=%s HTTP/1.1\r\n"
                 "Host: api.thingspeak.com\r\n"
                 "User-Agent: PicoW\r\n"
                 "Connection: close\r\n\r\n",
                 TALKBACK_ID, TALKBACK_API_KEY);

        memset(tlsClientResponseG, 0, sizeof(tlsClientResponseG));

        bool const success{run_tls_client_test(
            reinterpret_cast<uint8_t const *>(rootCaG),
            strlen(rootCaG) + 1,
            TLS_CLIENT_SERVER,
            request,
            TLS_CLIENT_TIMEOUT_SECS)};

        if (success)
        {
            int const newSetPoint{parseTalkBackCommand()};

            if (newSetPoint <= MAX_CO2_SETPOINT && newSetPoint >=
                MIN_CO2_SETPOINT)
            {
                this->storeSetpointToEEPROM(newSetPoint);
            }
        }
        else
        {
            printf(
                "[Cloud] TalkBack request failed, will retry next cycle.\n");
        }
    }
    else
    {
        printf(
            "[Cloud] Failed to acquire TLS mutex for checkTalkBackQueue\n");
    }
}


void CloudClass::dataSendTaskEntry(void *pvParametersP)
{
    auto *pSelf{static_cast<CloudClass *>(pvParametersP)};
    pSelf->dataSendTask();
}


[[noreturn]] void CloudClass::dataSendTask()
{
    printf("[DataSend] Task started\n");

    while (true)
    {
        if (!networkConnectedM)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!transmitM)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int co2Value, tempValue, humidityValue, fanSpeedValue,
            setpointValue;
        {
            const SensorValues readings{pResourcesM->getReadings()};

            co2Value = static_cast<int>(readings.co2M);
            tempValue = static_cast<int>(readings.temperatureM);
            humidityValue = static_cast<int>(readings.humidityM);
            fanSpeedValue = static_cast<int>(readings.fanSpeedM);
            setpointValue = static_cast<int>(readings.targetCo2M);
        }

        if (sendData(co2Value, tempValue, humidityValue,
                     fanSpeedValue, setpointValue))
        {
            printf("\n");
            printf("[DataSend] Transmission acknowledged.\n");
            printf("\n");
        }
        else
        {
            printf("[DataSend] Transmission failed or unverified.\n");
        }

        vTaskDelay(pdMS_TO_TICKS(DATA_SEND_DELAY));
    }
}


void CloudClass::talkbackPollTaskEntry(void *pvParametersP)
{
    auto *pSelf{static_cast<CloudClass *>(pvParametersP)};
    pSelf->talkbackPollTask();
}


[[noreturn]] void CloudClass::talkbackPollTask()
{
    printf("[TalkBack] Task started\n");

    while (!networkConnectedM)
    {
        if (ssidM[0] == '\0')
        {
            loadCredentialsFromEEPROM();
            if (ssidM[0] == '\0')
            {
                printf(
                    "[TalkBack] Waiting for WiFi credentials in EEPROM...\n");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        printf("[TalkBack] Network not connected, connecting...\n");
        connect();
        networkConnectedM = true;
        transmitM = true;
    }

    printf("[TalkBack] Starting command polling loop\n");
    while (true)
    {
        checkTalkBackQueue();
        vTaskDelay(pdMS_TO_TICKS(TALKBACK_POLL_DELAY));
    }
}
