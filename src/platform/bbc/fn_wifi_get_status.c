#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_get_status(fn_wifi_status_t *s)
{
    uint8_t req[] = {FN_WIFI_PROTOCOL_VERSION}, reply[128], result;
    uint16_t len = 0, at = 12;
    if (!s) return FN_ERR_INVALID;
    result = fn_bbc_wifi_call(FN_WIFI_CMD_GET_STATUS, req, 1, reply, sizeof(reply), &len);
    if (result != FN_OK) return result;
    if (len < 12 || reply[0] != FN_WIFI_PROTOCOL_VERSION) return FN_ERR_IO;
    memset(s, 0, sizeof(*s));
    s->link_state = reply[1]; s->configured_enabled = reply[2];
    s->bssid_valid = reply[3]; s->scan_supported = reply[4]; s->rssi = (int8_t)reply[5];
    memcpy(s->bssid.bytes, reply + 6, 6); s->bssid.valid = s->bssid_valid;
    if (!fn_bbc_wifi_string(reply, len, &at, s->ip, sizeof(s->ip)) ||
        !fn_bbc_wifi_string(reply, len, &at, s->subnet, sizeof(s->subnet)) ||
        !fn_bbc_wifi_string(reply, len, &at, s->gateway, sizeof(s->gateway)) ||
        !fn_bbc_wifi_string(reply, len, &at, s->dns, sizeof(s->dns))) return FN_ERR_IO;
    if (at + 3 <= len) {
        s->capabilities = (uint16_t)reply[at] | ((uint16_t)reply[at + 1] << 8);
        s->backend_kind = reply[at + 2];
        at = (uint16_t)(at + 3);
    }
    return at == len ? FN_OK : FN_ERR_IO;
}

