#include <string.h>
#include "fn_protocol.h"
#include "fn_raw.h"
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

uint8_t fn_fuji_get_info(fn_fuji_info_t *info) {
    uint8_t req[] = {FN_FUJI_PROTOCOL_VERSION};
    uint8_t reply[FN_FUJI_INFO_REPLY_MAX], result;
    uint16_t at = 1;
    fn_raw_response_t r;
    if (!info) return FN_ERR_INVALID;
    result = fn_raw_call(FN_DEVICE_FUJI, FN_FUJI_CMD_GET_INFO, req, 1,
                         reply, sizeof(reply), &r);
    if (result != FN_OK) return result;
    if (r.status == 2) return FN_ERR_INVALID;
    if (r.status == 8) return FN_ERR_UNSUPPORTED;
    if (r.status != 0) return FN_ERR_IO;
    if (r.payload_length < 1 || reply[0] == 0 || reply[0] > FN_FUJI_PROTOCOL_VERSION) return FN_ERR_IO;
    memset(info, 0, sizeof(*info));
    if (!take_string(reply, r.payload_length, &at, info->firmware, FN_FUJI_MAX_FIRMWARE_VERSION) ||
        !take_string(reply, r.payload_length, &at, info->profile, FN_FUJI_MAX_BUILD_PROFILE) ||
        at != r.payload_length) {
        memset(info, 0, sizeof(*info));
        return FN_ERR_IO;
    }
    return FN_OK;
}
