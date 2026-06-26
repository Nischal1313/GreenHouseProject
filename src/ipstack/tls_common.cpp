/*
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cassert>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/altcp_tcp.h>
#include <lwip/altcp_tls.h>
#include <lwip/dns.h>
#include <FreeRTOS.h>
#include <task.h>

// Forward declaration so callbacks can append to it before the actual definition
extern char tlsClientResponseG[2048*2];

struct TlsClientState {
    struct altcp_pcb *pcbM;
    bool completeM;
    int errorM;
    char const *httpRequestM;
    int timeoutM;
};

namespace {

altcp_tls_config *tlsConfigS = nullptr;

err_t tlsClientClose(void *argP)
{
    auto *state = static_cast<TlsClientState *>(argP);
    err_t err = ERR_OK;

    state->completeM = true;
    if (state->pcbM != nullptr)
    {
        altcp_arg(state->pcbM, nullptr);
        altcp_poll(state->pcbM, nullptr, 0);
        altcp_recv(state->pcbM, nullptr);
        altcp_err(state->pcbM, nullptr);
        err = altcp_close(state->pcbM);
        if (err != ERR_OK)
        {
            printf("close failed %d, calling abort\n", err);
            altcp_abort(state->pcbM);
            err = ERR_ABRT;
        }
        state->pcbM = nullptr;
    }
    return err;
}

err_t tlsClientConnected(void *argP, struct altcp_pcb *pcb, err_t err)
{
    auto *state = static_cast<TlsClientState *>(argP);
    if (err != ERR_OK)
    {
        printf("connect failed %d\n", err);
        return tlsClientClose(state);
    }

    printf("connected to server, sending request\n");
    err = altcp_write(state->pcbM, state->httpRequestM, strlen(state->httpRequestM), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        printf("error writing data, err=%d", err);
        return tlsClientClose(state);
    }

    return ERR_OK;
}

err_t tlsClientPoll(void *argP, struct altcp_pcb *pcb)
{
    auto *state = static_cast<TlsClientState *>(argP);
    printf("timed out\n");
    state->errorM = PICO_ERROR_TIMEOUT;
    return tlsClientClose(argP);
}

void tlsClientErr(void *argP, err_t err)
{
    auto *state = static_cast<TlsClientState *>(argP);
    printf("tlsClientErr %d\n", err);
    tlsClientClose(state);
    state->errorM = PICO_ERROR_GENERIC;
}

err_t tlsClientRecv(void *argP, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    auto *state = static_cast<TlsClientState *>(argP);
    if (!p)
    {
        printf("connection closed\n");
        return tlsClientClose(state);
    }

    if (p->tot_len > 0)
    {
        /* For simplicity this examples creates a buffer on stack the size of the data pending here,
           and copies all the data to it in one go.
           Do be aware that the amount of data can potentially be a bit large (TLS record size can be 16 KB),
           so you may want to use a smaller fixed size buffer and copy the data to it using a loop, if memory is a concern */
        char *buf = static_cast<char *>(malloc(p->tot_len + 1));

        pbuf_copy_partial(p, buf, p->tot_len, 0);
        buf[p->tot_len] = 0;

        printf("***\nnew data received from server:\n***\n\n%s\n", buf);

/* Append the received chunk into the global tlsClientResponseG buffer
          * so higher-level code (cloud_handler.cpp) can parse the full HTTP
         * response after run_tls_client_test() completes. Keep buffer bounds
          * in mind and do not overflow tlsClientResponseG. */
        size_t cur_len = strlen(tlsClientResponseG);
        size_t space_left = sizeof(tlsClientResponseG) - cur_len - 1; /* reserve NUL */
        if (space_left > 0)
        {
            /* Copy at most space_left bytes */
            size_t to_copy = (p->tot_len <= space_left) ? p->tot_len : space_left;
            memcpy(tlsClientResponseG + cur_len, buf, to_copy);
            tlsClientResponseG[cur_len + to_copy] = '\0';
        }
        else
        {
            /* Buffer full; consider logging or truncating further data */
            printf("tlsClientResponseG buffer full, dropping %u bytes\n", p->tot_len);
        }

        free(buf);

        altcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);

    return ERR_OK;
}

void tlsClientConnectToServerIp(ip_addr_t const *ipaddrP, TlsClientState *stateP)
{
    err_t err;
    u16_t port = 443;

    printf("connecting to server IP %s port %d\n", ipaddr_ntoa(ipaddrP), port);
    err = altcp_connect(stateP->pcbM, ipaddrP, port, tlsClientConnected);
    if (err != ERR_OK)
    {
        fprintf(stderr, "error initiating connect, err=%d\n", err);
        tlsClientClose(stateP);
    }
}

void tlsClientDnsFound(const char *hostnameP, ip_addr_t const *ipaddrP, void *argP)
{
    if (ipaddrP)
    {
        printf("DNS resolving complete\n");
        tlsClientConnectToServerIp(ipaddrP, static_cast<TlsClientState *>(argP));
    }
    else
    {
        printf("error resolving hostname %s\n", hostnameP);
        tlsClientClose(argP);
    }
}

bool tlsClientOpen(const char *hostnameP, void *argP)
{
    err_t err;
    ip_addr_t server_ip;
    auto *state = static_cast<TlsClientState *>(argP);

    state->pcbM = altcp_tls_new(tlsConfigS, IPADDR_TYPE_ANY);
    if (!state->pcbM)
    {
        printf("failed to create pcb\n");
        return false;
    }

    altcp_arg(state->pcbM, state);
    altcp_poll(state->pcbM, tlsClientPoll, state->timeoutM * 2);
    altcp_recv(state->pcbM, tlsClientRecv);
    altcp_err(state->pcbM, tlsClientErr);

    /* Set SNI */
    mbedtls_ssl_set_hostname(
        static_cast<mbedtls_ssl_context *>(altcp_tls_context(state->pcbM)),
        hostnameP);

    printf("resolving %s\n", hostnameP);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();

    err = dns_gethostbyname(hostnameP, &server_ip, tlsClientDnsFound, state);
    if (err == ERR_OK)
    {
        /* host is in DNS cache */
        tlsClientConnectToServerIp(&server_ip, state);
    }
    else if (err != ERR_INPROGRESS)
    {
        printf("error initiating DNS resolving, err=%d\n", err);
        tlsClientClose(state->pcbM);
    }

    cyw43_arch_lwip_end();

    return err == ERR_OK || err == ERR_INPROGRESS;
}

// Perform initialisation
TlsClientState *tlsClientInit(void)
{
    auto *state = static_cast<TlsClientState *>(calloc(1, sizeof(TlsClientState)));
    if (!state)
    {
        printf("failed to allocate state\n");
        return nullptr;
    }
    return state;
}

void tlsDebug(void *ctxP, int levelP, const char *fileP, int lineP, const char *messageP)
{
    fputs(messageP, stdout);
}

} // anonymous namespace

extern "C" {

bool run_tls_client_test(const uint8_t *cert, size_t cert_len, const char *server, const char *request, int timeout)
{
    //mbedtls_debug_set_threshold(4); // requires #define MBEDTLS_DEBUG_C in mbedtls_xonfig.h

    /* No CA certificate checking */
    tlsConfigS = altcp_tls_create_config_client(cert, cert_len);
    assert(tlsConfigS);

    //mbedtls_ssl_conf_authmode(&tls_config->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_authmode(reinterpret_cast<mbedtls_ssl_config *>(tlsConfigS), MBEDTLS_SSL_VERIFY_OPTIONAL);

    auto *state = tlsClientInit();
    if (!state)
    {
        return false;
    }
    state->httpRequestM = request;
    state->timeoutM = timeout;
    if (!tlsClientOpen(server, state))
    {
        return false;
    }
    while (!state->completeM)
    {
        // the following #ifdef is only here so this same example can be used in multiple modes;
        // you do not need it in your code
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        // you can poll as often as you like, however if you have nothing else to do you can
        // choose to sleep until either a specified time, or cyw43_arch_poll() has work to do:
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
#else
        // if you are not using pico_cyw43_arch_poll, then WiFI driver and lwIP work
        // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // work you might be doing.
        //sleep_ms(1000);
        vTaskDelay(1000);
#endif
    }
    int err = state->errorM;
    free(state);
    altcp_tls_free_config(tlsConfigS);
    return err == 0;
}

} // extern "C"

// ============================================================================
// Global TLS variables for ThingSpeak / HTTPS requests
// ============================================================================

// Response buffer (used by run_tls_client_test and cloud_handler)
char tlsClientResponseG[2048 * 2];

// Root CA certificate for api.thingspeak.com (Amazon Root CA 1)
extern char const rootCaG[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTExMDIxMjAwMDAwMFoXDTM5MTIzMTIzNTk1OVowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALwDaQkM6ECF\n"
"m+J8UqgD5aPq4lOK3yKddrdztzD8ix8qO6V8UghYbNvD0Z8VKxXzE7t3Eo4wxIhX\n"
"QAGKx9e1Fj0dxz4J1TxRe+VY6qzY4k7RwlHk2eZ0zQ4ROmu+EMB+qVhjP0+lXKpT\n"
"nK7xj+Ubv9kRkSbtf0jcl+H4R68FV/H2m2C1Q3Z1z0zQOYqav4PzoKzGozA6J0T0\n"
"0G9vnjNjaT9JKfH1sxQ1q0x6s+JWu1Wj6a+upYjF8hHk/hx8m5qVcfuZ8O6q3K/Y\n"
"t5ek/jexxD5X+oElhYQgJQ3H82Rb7Dn8uF3MBS5Y9xLw1mW4BLUfrxzJYcZ8J9PR\n"
"g1jMUaO+gn8CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUK9TR\n"
"4L8C7jqdPjd5P5umgwywXDUwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3DQEBCwUA\n"
"A4IBAQBLVQmIMKj2kB+awgaqE6xkXkG1PbQ5i3h0aoyS+9g7p2b+zHkLwbXBRCAJ\n"
"fS2bXyH8BGFBR9rnGzDe9lEddYj0XKx2pDD7Z6FHYh0kds6TkISaUh2uZd80WInA\n"
"yqUuL3WUk6o5qSPk0nC6I4r3s3POZ+4tO8phufQ/H99/txS2i8Gl3cEw1bdIKpMh\n"
"bF8gz5C3rW4znvL+3r4K8P5k2Aq1wCfsH7O3QFlu3hTrdxg5RE9X3ToVJZW9njsV\n"
"6NROzUP7wp3ULg0NlddM4EcbmgSizHcooR4vFv8rxuU3RQqY08b0v5FYvysG+5WJ\n"
"0adqTdD1/o0dVv6b3TmbVN2+LDW8\n"
"-----END CERTIFICATE-----\n";
