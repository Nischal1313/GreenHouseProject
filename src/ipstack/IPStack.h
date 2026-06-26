//
// Created by Keijo Länsikunnas on 12.2.2024.
//

#pragma once

#include <cstdint>
#include <memory>
#include <array>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <lwip/pbuf.h>
#include <lwip/tcp.h>


class IPStack
{
public:
    IPStack(const char *ssidP, const char *pwP);
    int connect(const char *hostnameP, int portP);
    int connect(uint32_t hostnameP, int portP);
    int read(unsigned char *bufferP, int lenP, int timeoutP);
    int write(unsigned char *bufferP, int lenP, int timeoutP);
    int disconnect();
    // lwip callback functions
    static err_t tcpClientSent(void *argP, struct tcp_pcb *tpcbP, u16_t lenP);
    static err_t tcpClientPoll(void *argP, struct tcp_pcb *tpcbP);
    static void tcpClientErr(void *argP, err_t errP);
    static err_t tcpClientRecv(void *argP, struct tcp_pcb *tpcbP, struct pbuf *pP, err_t errP);
    static err_t tcpClientConnected(void *argP, struct tcp_pcb *tpcbP, err_t errP);

    static int const BUF_SIZE{2048};
    static int const POLL_TIME_S{5};

private:
    struct tcp_pcb *pTcpPcbM;
    ip_addr_t remoteAddrM;
    std::array<uint8_t, BUF_SIZE> bufferM;
    uint16_t countM;
    uint32_t droppedM;
    uint16_t wrM; // write index
    uint16_t rdM; // read index
    bool connectedM;
};
