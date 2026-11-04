#include <string.h>

#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fujinet-nio.h"

static uint8_t wifi_status(uint8_t s)
{
    if (s == 0) return FN_OK;
    if (s == 2) return FN_ERR_INVALID;
    if (s == 3) return FN_ERR_BUSY;
    if (s == 4) return FN_ERR_NOT_READY;
    if (s == 8) return FN_ERR_UNSUPPORTED;
    return FN_ERR_IO;
}

uint8_t fn_bbc_wifi_call(uint8_t command, const uint8_t *request, uint16_t request_len,
                         uint8_t *reply, uint16_t reply_capacity, uint16_t *reply_len)
{
    uint16_t ignored_len;
    uint8_t status = FN_ERR_INTERNAL;
    uint8_t result;
    if (!reply_len) reply_len = &ignored_len;
    result = fn_bbc_device_call_raw(FN_DEVICE_WIFI, command, request, request_len,
                                            reply, reply_capacity, &status, reply_len);
    return result == FN_OK ? wifi_status(status) : result;
}

static uint8_t get_u8(const uint8_t *p, uint16_t len, uint16_t *at, uint8_t *v)
{
    if (*at >= len) return 0;
    *v = p[(*at)++];
    return 1;
}

uint8_t fn_bbc_wifi_string(const uint8_t *p, uint16_t len, uint16_t *at, char *out, uint8_t cap)
{
    uint8_t n;
    if (!get_u8(p, len, at, &n) || n >= cap || (uint16_t)(*at + n) > len) return 0;
    memcpy(out, p + *at, n);
    out[n] = 0;
    *at = (uint16_t)(*at + n);
    return 1;
}

