//
// Created by Amaan on 16/09/2025.
// None of this is relevant yet
//

#include "cloud.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/sockets.h"
#include <cstdio>
#include <string>

#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define THINGSPEAK_HOST "api.thingspeak.com"
#define WRITE_API_KEY "YOUR_WRITE_API_KEY"

PicotoCloud::PicotoCloud(int co2, int rh, float temp, int fan, int setpoint) {
    send_data(co2, rh, temp, fan, setpoint);
}

void send_data(int co2, int rh, float temp, int fan, int setpoint) {
    int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = PP_HTONS(80);

    if (lwip_connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        lwip_close(sock);
        return;
    }

    char body[200];
    snprintf(body, sizeof(body),
        "api_key=%s&field1=%d&field2=%d&field3=%.1f&field4=%d&field5=%d",
        WRITE_API_KEY, co2, rh, temp, fan, setpoint);

    char request[400];
    snprintf(request, sizeof(request),
        "POST /update HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %d\r\n\r\n"
        "%s",
        THINGSPEAK_HOST, (int)strlen(body), body);

    lwip_write(sock, request, strlen(request));
    lwip_close(sock);
}

/*things to do in main
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);

    while (true) {
        // Example sensor values
        send_data(450, 55, 22.5, 60, 500);
        sleep_ms(20000); // Wait 20 seconds between updates
    }
}
*/