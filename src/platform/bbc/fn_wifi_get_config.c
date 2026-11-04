#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_get_config(fn_wifi_config_t *c)
{
    uint8_t req[] = {FN_WIFI_PROTOCOL_VERSION}, reply[96], result;
    uint16_t len = 0, at = 3;
    if (!c) return FN_ERR_INVALID;
    result = fn_bbc_wifi_call(FN_WIFI_CMD_GET_CONFIG, req, 1, reply, sizeof(reply), &len);
    if (result != FN_OK) return result;
    if (len < 3 || reply[0] != FN_WIFI_PROTOCOL_VERSION) return FN_ERR_IO;
    memset(c, 0, sizeof(*c)); c->enabled = reply[1]; c->password_present = reply[2];
    if (!fn_bbc_wifi_string(reply, len, &at, c->ssid, sizeof(c->ssid)) ||
        !fn_bbc_wifi_string(reply, len, &at, c->bssid, sizeof(c->bssid)) || at != len) return FN_ERR_IO;
    return FN_OK;
}

