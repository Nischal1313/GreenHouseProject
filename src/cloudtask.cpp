//
// Created by Amaan on 26/09/2025.
//


#include <string>
#include "cloudtask.h"



CloudClass::CloudClass(const std::shared_ptr<SharedResources>& sharedResources): resources(sharedResources){
    Co2_SetPoint = 0;
}

void CloudClass::init() {
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

}

void CloudClass::setCredentials(const char* ssid, const char* password) {
    strcpy(this->ssid, ssid);
    strcpy(this->password, password);
}

void CloudClass::connect() {
    init();
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect\n");
        return;
    }
}

void CloudClass::recieve() {
    run_tls_client_test(NULL, 0, TLS_CLIENT_SERVER, req, TLS_CLIENT_TIMEOUT_SECS);
    Co2_SetPoint = get_co2_setpoint();
    printf("Co2_SetPoint: %d\n",Co2_SetPoint);
}

void CloudClass::send( int co2, int hum, int tem, int fan, int co2sp) {
    char resp[256];
    sprintf(resp, "POST /update?api_key=65BAWC3Q8R5MODFH&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d HTTP/1.1\r\n"
                 "Host: api.thingspeak.com\r\n"
                 "\r\n", co2,hum, tem, fan, co2sp);
    run_tls_client_test(NULL, 0, TLS_CLIENT_SERVER, resp, TLS_CLIENT_TIMEOUT_SECS);
}

// This function we are using to send and receive data from the server togatehr
void CloudClass::sendAndreceive(int co2, int tem, int rh, int fanSpeed, int co2sp) {
    char resp[256];
    sprintf(resp, "POST https://api.thingspeak.com/update.json?field1=%d&field2=%d&field3=%d&field4=%d&field5=%d&api_key=ROBHS91KLA5EH4ZO&talkback_key=65BAWC3Q8R5MODFH HTTP/1.1\r\n"
                 "Host: api.thingspeak.com\r\n"
                 "Connection: close\r\n"
                 "\r\n", co2, tem, rh, fanSpeed, co2sp);

    run_tls_client_test(NULL, 0, TLS_CLIENT_SERVER, resp, TLS_CLIENT_TIMEOUT_SECS);
    Co2_SetPoint = get_co2_setpoint();
    printf("Co2_SetPoint: %d\n",Co2_SetPoint);
}
