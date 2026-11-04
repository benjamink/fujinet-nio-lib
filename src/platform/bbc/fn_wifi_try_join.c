#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_try_join(const char *ssid, const char *password)
{
    uint8_t request[3 + FN_WIFI_MAX_SSID + FN_WIFI_MAX_PASSWORD];
    uint16_t ssid_len, password_len, at, reply_len = 0;
    uint8_t result;
    if (!ssid || !password) return FN_ERR_INVALID;
    ssid_len = (uint16_t) strlen(ssid);
    password_len = (uint16_t) strlen(password);
    if (!ssid_len || ssid_len > FN_WIFI_MAX_SSID || password_len > FN_WIFI_MAX_PASSWORD)
        return FN_ERR_INVALID;
    request[0] = FN_WIFI_PROTOCOL_VERSION;
    request[1] = (uint8_t) ssid_len;
    memcpy(request + 2, ssid, ssid_len);
    at = (uint16_t) (2 + ssid_len);
    request[at++] = (uint8_t) password_len;
    memcpy(request + at, password, password_len);
    at = (uint16_t) (at + password_len);
    result = fn_bbc_wifi_call(FN_WIFI_CMD_TRY_JOIN, request, at, request, sizeof(request), &reply_len);
    if (result != FN_OK) return result;
    return reply_len && request[0] == FN_WIFI_PROTOCOL_VERSION ? FN_OK : FN_ERR_IO;
}

