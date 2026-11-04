#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_set_config(const fn_wifi_config_update_t *u)
{
    uint8_t req[2 + 1 + FN_WIFI_MAX_SSID + 1 + FN_WIFI_MAX_BSSID + 1 + FN_WIFI_MAX_PASSWORD];
    const char *v[3]; const uint8_t f[3] = {FN_WIFI_SET_SSID, FN_WIFI_SET_BSSID, FN_WIFI_SET_PASSWORD};
    uint16_t at = 2, n; uint8_t i;
    if (!u || (u->fields & (uint8_t)~0x3Fu)) return FN_ERR_INVALID;
    req[0] = FN_WIFI_PROTOCOL_VERSION; req[1] = u->fields;
    if (u->fields & FN_WIFI_SET_ENABLED) req[at++] = u->enabled ? 1 : 0;
    v[0] = u->ssid; v[1] = u->bssid; v[2] = u->password;
    for (i = 0; i < 3; ++i) if (u->fields & f[i]) {
        if (!v[i]) return FN_ERR_INVALID;
        n = (uint16_t)strlen(v[i]);
        if ((i == 0 && n > FN_WIFI_MAX_SSID) || (i == 1 && n != 0 && n != FN_WIFI_MAX_BSSID) ||
            (i == 2 && n > FN_WIFI_MAX_PASSWORD) || n > 255) return FN_ERR_INVALID;
        req[at++] = (uint8_t)n; memcpy(req + at, v[i], n); at = (uint16_t)(at + n);
    }
    return fn_bbc_wifi_call(FN_WIFI_CMD_SET_CONFIG, req, at, req, sizeof(req), 0);
}

