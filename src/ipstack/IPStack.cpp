//
// Created by Keijo Länsikunnas on 12.2.2024.
//
#include <cstring>

#include <pico/time.h>

#include "IPStack.h"


// To remove Pico example debugging functions during refactoring
//#define DEBUG_printf(x, ...) {}
#define DEBUG_printf printf
#define DUMP_BYTES(A, B) {}


IPStack::IPStack(const char *ssidP, const char *pwP)
    : pTcpPcbM{nullptr}, droppedM{0}, countM{0}, wrM{0}, rdM{0}, connectedM{false}
{
    if (cyw43_arch_init())
    {
        DEBUG_printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    DEBUG_printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(ssidP, pwP, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        DEBUG_printf("Failed to connect.\n");
    }
    else
    {
        DEBUG_printf("Connected.\n");
    }
}

int IPStack::connect(uint32_t hostnameP, int portP)
{
    (void)hostnameP;
    (void)portP;
    return ERR_ARG;
}

int IPStack::connect(const char *hostnameP, int portP)
{
    // check if the hostname requires DNS resolution
    if (!ip4addr_aton(hostnameP, &remoteAddrM))
    {
        // dns not implemented yet
        return ERR_ARG;
    }
    // open a socket connection
    DEBUG_printf("Connecting to %s port %u\n", ip4addr_ntoa(&remoteAddrM), portP);
    pTcpPcbM = tcp_new_ip_type(IP_GET_TYPE(remoteAddrM));
    if (!pTcpPcbM)
    {
        DEBUG_printf("failed to create pcb\n");
        return ERR_MEM;
    }

    tcp_arg(pTcpPcbM, this);
    tcp_poll(pTcpPcbM, IPStack::tcpClientPoll, POLL_TIME_S * 2);
    tcp_sent(pTcpPcbM, IPStack::tcpClientSent);
    tcp_recv(pTcpPcbM, IPStack::tcpClientRecv);
    tcp_err(pTcpPcbM, IPStack::tcpClientErr);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(pTcpPcbM, &remoteAddrM, portP, IPStack::tcpClientConnected);
    cyw43_arch_lwip_end();

    return err;
}

err_t IPStack::tcpClientSent(void *argP, struct tcp_pcb *tpcbP, u16_t lenP)
{
    //auto state = static_cast<IPStack *>(argP);
    (void)argP;
    (void)tpcbP;
    (void)lenP;
    DEBUG_printf("tcp_client_sent %u\n", lenP);

    return ERR_OK;
}

err_t IPStack::tcpClientConnected(void *argP, struct tcp_pcb *tpcbP, err_t errP)
{
    auto *state = static_cast<IPStack *>(argP);
    (void)tpcbP;
    if (errP != ERR_OK)
    {
        printf("connect failed %d\n", errP);
    }
    state->connectedM = true;

    return ERR_OK;
}

err_t IPStack::tcpClientPoll(void *argP, struct tcp_pcb *tpcbP)
{
    //auto state = static_cast<IPStack *>(argP);
    (void)argP;
    (void)tpcbP;
    DEBUG_printf("tcp_client_poll\n");
    return ERR_OK;
}

void IPStack::tcpClientErr(void *argP, err_t errP)
{
    //auto state = static_cast<IPStack *>(argP);
    (void)argP;
    if (errP != ERR_ABRT)
    {
        DEBUG_printf("tcp_client_err %d\n", errP);
        //state->tcp_result(err);
    }
}

err_t IPStack::tcpClientRecv(void *argP, struct tcp_pcb *tpcbP, struct pbuf *pP, err_t errP)
{
    auto *state = static_cast<IPStack *>(argP);
    (void)errP;
    if (!pP)
    {
        // connection has been closed - do we need to react to this somehow?
        return ERR_OK;
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (pP->tot_len > 0)
    {
        // Receive the buffer
        uint16_t available = BUF_SIZE - state->countM;
        uint16_t bytes_to_copy = available > pP->tot_len ? pP->tot_len : available;
        uint16_t wr_end = state->wrM + bytes_to_copy;
        uint16_t first_copy = 0;

        // check if bytes are to be dropped
        if (bytes_to_copy < pP->tot_len)
        {
            state->droppedM += pP->tot_len - bytes_to_copy;
        }

        if (wr_end > BUF_SIZE)
        {
            // need to copy in two parts
            first_copy = BUF_SIZE - state->wrM; // calculate the size of first part to copy
            if (first_copy)
            {
                bytes_to_copy -= pbuf_copy_partial(pP, state->bufferM.data() + state->wrM, first_copy, 0);
                state->wrM = 0; // start next copy from beginning
            }
            state->countM += first_copy; // increment count by copied bytes
        }
        state->wrM += pbuf_copy_partial(pP, state->bufferM.data() + state->wrM, bytes_to_copy, first_copy);
        state->wrM %= BUF_SIZE; // wrap over
        state->countM += bytes_to_copy; // increment count by the rest of the copied bytes

        tcp_recved(tpcbP, pP->tot_len);
    }
    pbuf_free(pP); // can we omit this call instead of dropping bytes to save the buffer for copying the rest later?

    return ERR_OK;
}


int IPStack::read(unsigned char *bufferP, int lenP, int timeoutP)
{
    // is it possible to call with zero timeout?
    auto to = make_timeout_time_ms(timeoutP);
    do
    {
        cyw43_arch_poll();
    } while (countM < lenP && !time_reached(to));

    uint16_t first_copy = 0;
    int bytes_to_copy = countM < lenP ? countM : lenP;
    if (bytes_to_copy)
    {
        uint16_t rd_end = rdM + bytes_to_copy;
        if (rd_end > BUF_SIZE)
        {
            // need to copy in two parts
            first_copy = BUF_SIZE - rdM; // calculate the size of the first part to copy
            if (first_copy)
            {
                std::memcpy(bufferP, bufferM.data() + rdM, first_copy);
                bytes_to_copy -= first_copy;
                countM -= first_copy; // reduce count by copied bytes
            }
            // start from beginning
            std::memcpy(bufferP + first_copy, bufferM.data(), bytes_to_copy);
            rdM = bytes_to_copy;
        }
        else
        {
            std::memcpy(bufferP, bufferM.data() + rdM, bytes_to_copy);
            rdM = (rdM + bytes_to_copy) % BUF_SIZE;
        }
        countM -= bytes_to_copy; // reduce count by the rest of the copied bytes
    }
    // return count of copied bytes
    return bytes_to_copy + first_copy;
}

int IPStack::write(unsigned char *bufferP, int lenP, int timeoutP)
{
    int rv = lenP;
    (void)timeoutP;
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();

    err_t err = tcp_write(pTcpPcbM, bufferP, lenP, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        DEBUG_printf("Failed to write data %d\n", err);
        rv = -1;
    }
    // headers suggest that this should be called to make sure that data is sent right away
    // however there is TCB_WRITE_FLAG_MORE that possibly indicates the same thing??
    if (tcp_output(pTcpPcbM) != ERR_OK)
    {
        // failed! What should I do now?
        rv = -2;
    }

    cyw43_arch_lwip_end();

    return rv;
}

int IPStack::disconnect()
{
    cyw43_arch_lwip_begin();

    err_t err = ERR_OK;
    if (pTcpPcbM != nullptr)
    {
        tcp_arg(pTcpPcbM, nullptr);
        tcp_poll(pTcpPcbM, nullptr, 0);
        tcp_sent(pTcpPcbM, nullptr);
        tcp_recv(pTcpPcbM, nullptr);
        tcp_err(pTcpPcbM, nullptr);
        err = tcp_close(pTcpPcbM);
        if (err != ERR_OK)
        {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(pTcpPcbM); // this deallocates tcp_pcb
            err = ERR_ABRT;
        }
        pTcpPcbM = nullptr;
    }
    cyw43_arch_lwip_end();
    return err;
}
