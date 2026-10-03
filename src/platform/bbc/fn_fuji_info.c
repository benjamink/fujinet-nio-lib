#include <string.h>

#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fujinet-nio.h"

/* We ask for the highest version we understand; the reply uses the lower of
 * that and the firmware's, and a version-1 reply is exactly: version,
 * firmware (u8 length + text), build profile (u8 length + text). See
 * "Extending commands" in fujinet-nio's protocol_reference.md. */
#define FN_FUJI_INFO_REPLY_MAX (1 + 1 + FN_FUJI_MAX_FIRMWARE_VERSION + 1 + FN_FUJI_MAX_BUILD_PROFILE)

static uint8_t take_string(const uint8_t *reply, uint16_t len, uint16_t *at, char *out, uint8_t cap)
{
    uint8_t n;
    if (*at >= len) return 0;
    n = reply[*at];
    if (n > cap || (uint16_t)(*at + 1 + n) > len) return 0;
    memcpy(out, reply + *at + 1, n);
    out[n] = 0;
    *at = (uint16_t)(*at + 1 + n);
    return 1;
}

uint8_t fn_fuji_get_info(fn_fuji_info_t *info)
{
    uint8_t req[] = {FN_FUJI_PROTOCOL_VERSION};
    uint8_t reply[FN_FUJI_INFO_REPLY_MAX], result, status = FN_ERR_INTERNAL;
    uint16_t len = 0, at = 1;
    if (!info) return FN_ERR_INVALID;
    result = fn_bbc_device_call_raw(FN_DEVICE_FUJI, FN_FUJI_CMD_GET_INFO, req, 1,
                                    reply, sizeof(reply), &status, &len);
    if (result != FN_OK) return result;
    if (status == 2) return FN_ERR_INVALID;
    if (status == 8) return FN_ERR_UNSUPPORTED;
    if (status != 0) return FN_ERR_IO;
    if (len < 1 || reply[0] == 0 || reply[0] > FN_FUJI_PROTOCOL_VERSION) return FN_ERR_IO;
    memset(info, 0, sizeof(*info));
    if (!take_string(reply, len, &at, info->firmware, FN_FUJI_MAX_FIRMWARE_VERSION) ||
        !take_string(reply, len, &at, info->profile, FN_FUJI_MAX_BUILD_PROFILE) ||
        at != len) {
        memset(info, 0, sizeof(*info));
        return FN_ERR_IO;
    }
    return FN_OK;
}
