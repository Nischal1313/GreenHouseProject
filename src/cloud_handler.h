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


class CloudClass
{
public:
    CloudClass(std::shared_ptr<SensorHandler> const &pSensorHandlerP,
               SemaphoreHandle_t const eepromMutexP,
               Eeprom &rEepromP);

    void storeSetpointToEEPROM(int setpointP) const;

    void connect();

    void setCredentials(char const *pSsidP, char const *pPasswordP);

    void loadCredentialsFromEEPROM();

    bool sendData(int co2P, int temperatureP, int humidityP,
                  int fanSpeedP, int co2SetpointP);

    void checkTalkBackQueue() const;

    static void dataSendTaskEntry(void *pvParametersP);
    static void talkbackPollTaskEntry(void *pvParametersP);

    [[noreturn]] void dataSendTask();
    [[noreturn]] void talkbackPollTask();

    bool transmitM{false};
    bool networkConnectedM{false};

    // Mutex to protect TLS operations (shared between both tasks)
    static SemaphoreHandle_t tlsMutexS;

private:
    static constexpr uint16_t EEPROM_WIFI_NAME_ADDR{0x0500};
    static constexpr uint16_t EEPROM_WIFI_PASSWD_ADDR{0x0600};
    static constexpr uint16_t FIELD_SIZE{32};
    static constexpr uint DATA_SEND_DELAY{60000};
    static constexpr uint TALKBACK_POLL_DELAY{5000};
    static constexpr int MIN_CO2_SETPOINT{200};
    static constexpr int MAX_CO2_SETPOINT{15000};
    static constexpr uint16_t EEPROM_CO2_CLOUD_ADDR{0x0200};
    static constexpr uint8_t TLS_CLIENT_TIMEOUT_SECS{15};
    static constexpr char const *TLS_CLIENT_SERVER{"api.thingspeak.com"};

    char ssidM[32]{};
    char passwordM[64]{};
    std::shared_ptr<SensorHandler> pResourcesM;
    Eeprom *pEepromM;
    SemaphoreHandle_t eepromMutexM;

    static int parseTalkBackCommand();
};


extern "C" {
bool run_tls_client_test(uint8_t const *pCertP, size_t certLenP,
                         char const *pServerP, char const *pRequestP,
                         int timeoutP);
}

extern char tlsClientResponseG[2048*2];
extern char const rootCaG[];
