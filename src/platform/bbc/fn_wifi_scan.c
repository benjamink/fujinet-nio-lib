#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_scan(uint16_t offset, uint8_t limit, fn_wifi_scan_record_t *r,
                     uint8_t cap, uint8_t *count, uint8_t *more,
                     uint8_t *reply, uint16_t reply_capacity)
{
    uint8_t req[4], result, n, i, page_limit;
    uint16_t len, at, max_records;
    if (!r || !count || !more || !cap || !limit ||
        !reply || reply_capacity < (uint16_t)(3 + 1 + FN_WIFI_MAX_SSID + 9) ||
        limit > FN_WIFI_MAX_SCAN_RECORDS)
        return FN_ERR_INVALID;
    max_records = (uint16_t)((reply_capacity - 3) / (1 + FN_WIFI_MAX_SSID + 9));
    if (max_records > FN_WIFI_MAX_SCAN_RECORDS) max_records = FN_WIFI_MAX_SCAN_RECORDS;
    page_limit = limit;
    if ((uint16_t)page_limit > max_records) page_limit = (uint8_t)max_records;
    if (!page_limit) return FN_ERR_INVALID;
    req[0] = FN_WIFI_PROTOCOL_VERSION;
    req[1] = (uint8_t)offset;
    req[2] = (uint8_t)(offset >> 8);
    req[3] = page_limit;
    len = 0; at = 3;
    result = fn_bbc_wifi_call(FN_WIFI_CMD_SCAN, req, 4, reply, reply_capacity, &len);
    if (result != FN_OK) return result;
    if (len < 3 || reply[0] != FN_WIFI_PROTOCOL_VERSION) return FN_ERR_IO;
    *more = reply[1]; n = reply[2]; if (n > cap || n > page_limit) return FN_ERR_INVALID;
    for (i = 0; i < n; ++i) {
        memset(&r[i], 0, sizeof(r[i]));
        if (!fn_bbc_wifi_string(reply, len, &at, r[i].ssid, sizeof(r[i].ssid)) || at + 9 > len) return FN_ERR_IO;
        memcpy(r[i].bssid.bytes, reply + at, 6); r[i].bssid.valid = 1; at += 6;
        r[i].rssi = (int8_t)reply[at++]; r[i].channel = reply[at++]; r[i].auth = reply[at++];
    }
    *count = n; return at == len ? FN_OK : FN_ERR_IO;
}

