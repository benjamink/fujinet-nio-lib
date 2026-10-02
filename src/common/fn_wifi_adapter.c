#include <string.h>
#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

/* Kept apart from fn_wifi.c so programs that do not show adapter details do
 * not link it. Reply: version, MAC-present, MAC[6], firmware length, text. */
uint8_t fn_wifi_get_adapter_info(fn_wifi_adapter_info_t *info) {
    uint8_t req[] = {FN_WIFI_PROTOCOL_VERSION};
    uint8_t reply[9 + FN_WIFI_MAX_FIRMWARE_VERSION], result, n;
    uint16_t len;
    fn_raw_response_t r;
    if (!info) return FN_ERR_INVALID;
    result = fn_raw_call(FN_DEVICE_WIFI, FN_WIFI_CMD_GET_ADAPTER_INFO, req, 1,
                         reply, sizeof(reply), &r);
    if (result != FN_OK) return result;
    if (r.status == 2) return FN_ERR_INVALID;
    if (r.status == 4) return FN_ERR_NOT_READY;
    if (r.status == 8) return FN_ERR_UNSUPPORTED;
    if (r.status != 0) return FN_ERR_IO;
    len = r.payload_length;
    if (len < 9 || reply[0] != FN_WIFI_PROTOCOL_VERSION) return FN_ERR_IO;
    n = reply[8];
    if (n > FN_WIFI_MAX_FIRMWARE_VERSION || (uint16_t)(9 + n) != len) return FN_ERR_IO;
    memset(info, 0, sizeof(*info));
    info->mac.valid = reply[1] ? 1 : 0;
    memcpy(info->mac.bytes, reply + 2, 6);
    memcpy(info->firmware, reply + 9, n);
    info->firmware[n] = 0;
    return FN_OK;
}
