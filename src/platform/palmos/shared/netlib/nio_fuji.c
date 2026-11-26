/* FujiDevice (0x70) GetInfo and the Wi-Fi service (0xF3) status and config.
 * There is no Wi-Fi radio to manage: the Palm's own Network preferences own
 * the link (Bluetooth, a modem, a cradle), so the Wi-Fi service only reports
 * whether the stack is up and at which address. */
#include <string.h>
#include "nio.h"
#include "nio_sock.h"

/* Text constants the library audit (scripts/palmos_shared_image.py) checks byte for byte: objdump decodes them
 * as instructions, some through A5. */
static const char k_netlib_device[] = "Palm NetLib";

#define FUJI_GET_INFO 0x01
#define WIFI_GET_STATUS 0x01
#define WIFI_GET_CONFIG 0x02

#define WIFI_LINK_DOWN 0
#define WIFI_LINK_CONNECTED 2
#define WIFI_CAP_STATUS 0x0002
#define WIFI_CAP_HOST   0x0040
#define WIFI_BACKEND_HOST 2

static uint16_t put_string(uint8_t *out, const char *text)
{
    uint8_t n = (uint8_t)strlen(text);

    out[0] = n;
    memcpy(out + 1, text, n);
    return (uint16_t)(1 + n);
}

uint8_t nio_fuji(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                 uint8_t *out, uint16_t cap, uint16_t *out_len)
{
    uint16_t at = 1;

    (void)nio;
    if (cmd != FUJI_GET_INFO) return NIO_ST_UNSUPPORTED;
    if (in_len < 1 || in[0] == 0) return NIO_ST_INVALID;
    if (cap < 3 + sizeof(NIO_VERSION_STRING) + sizeof(NIO_PROFILE_STRING)) return NIO_ST_INVALID;
    out[0] = 1;
    at = (uint16_t)(at + put_string(out + at, NIO_VERSION_STRING));
    at = (uint16_t)(at + put_string(out + at, NIO_PROFILE_STRING));
    *out_len = at;
    return NIO_ST_OK;
}

uint8_t nio_wifi(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                 uint8_t *out, uint16_t cap, uint16_t *out_len)
{
    char ip[20];
    uint16_t at;
    uint8_t up;

    if (in_len < 1 || in[0] != 1) return NIO_ST_INVALID;
    if (cap < 64) return NIO_ST_INVALID;
    switch (cmd) {
    case WIFI_GET_STATUS:
        /* Report the link as it is. Opening NetLib here would make the
         * system dial, or show its own "Incomplete setup" alert when no
         * network service is set up; only a connection should do that. */
        up = nio->net_up;
        ip[0] = 0;
        if (up) nio_sock_local_ip(&nio->net, ip, sizeof(ip));
        memset(out, 0, 12);
        out[0] = 1;
        out[1] = (uint8_t)(up ? WIFI_LINK_CONNECTED : WIFI_LINK_DOWN);
        out[2] = 1;
        at = 12;
        at = (uint16_t)(at + put_string(out + at, ip));
        at = (uint16_t)(at + put_string(out + at, ""));
        at = (uint16_t)(at + put_string(out + at, ""));
        at = (uint16_t)(at + put_string(out + at, ""));
        nio_put16(out + at, WIFI_CAP_STATUS | WIFI_CAP_HOST);
        out[at + 2] = WIFI_BACKEND_HOST;
        *out_len = (uint16_t)(at + 3);
        return NIO_ST_OK;
    case WIFI_GET_CONFIG:
        out[0] = 1;
        out[1] = 1;
        out[2] = 0;
        at = 3;
        at = (uint16_t)(at + put_string(out + at, k_netlib_device));
        at = (uint16_t)(at + put_string(out + at, ""));
        *out_len = at;
        return NIO_ST_OK;
    default:
        return NIO_ST_UNSUPPORTED;
    }
}
