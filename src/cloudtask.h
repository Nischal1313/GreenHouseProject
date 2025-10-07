//
// Created by Amaan on 26/09/2025.
//

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
#include "SharedResources.h"

struct SharedResources;

#define TLS_CLIENT_SERVER        "api.thingspeak.com"

#define TLS_CLIENT_TIMEOUT_SECS  15


extern "C" {
    bool run_tls_client_test(const uint8_t *cert, size_t cert_len, const char *server, const char *request, int timeout);
    int get_co2_setpoint();
}

class CloudClass {
public:
    CloudClass(const std::shared_ptr<SharedResources>& sharedResources );
    void init();
    void connect();
    void recieve();
    void send(int co2, int tem, int rh, int fanSpeed, int Co2_SetPoint);
    void sendAndreceive(int co2, int tem, int rh, int fanSpeed, int Co2_SetPoint);
    void setCredentials(const char* ssid, const char* password);
    int Co2_SetPoint;
    bool transmit = false;

private:
    char ssid[32];
    char password[32];
    std::shared_ptr<SharedResources> resources;
    uint32_t event;

    const char *req = "POST /talkbacks/55419/commands/execute.json HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "User-Agent: PicoW\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: 24\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n"
                      "api_key=65BAWC3Q8R5MODFH";


    const char *req2 = "POST /update.json HTTP/1.1\r\n"
                      "Host: api.thingspeak.com\r\n"
                      "User-Agent: PicoW\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: 65\r\n"
                      "Content-Type: application/x-www-form-urlencoded\r\n"
                      "\r\n"
                      "field1=370&api_key=1WWH2NWXSM53URR5&talkback_key=371DAWENQKI6J8DD";


};


#endif //CLOUDTASK_H


