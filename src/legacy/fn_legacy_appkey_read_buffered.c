#include <string.h>

#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_protocol.h"
#include "fn_raw.h"
#include "fn_legacy_appkey_internal.h"

/* The transfer buffer is caller-owned and must not overlap the output. */
bool fuji_read_appkey_buffered(uint8_t key_id, uint16_t *count, uint8_t *data,
                               uint8_t *transfer, uint16_t transfer_size)
{
    uint16_t capacity, data_len;
    fn_raw_response_t raw;

    if (count == 0 || data == 0 || transfer == 0 || _fn_legacy_appkey_creator_id == 0)
        return false;
    *count = 0;
    capacity = _fn_legacy_appkey_capacity();
    if (transfer_size < (uint16_t)(FN_LEGACY_APPKEY_READ_RESPONSE_HEADER + capacity)) return false;
    /* The legacy URI is always 31 bytes. Build it directly into the
       caller's request to avoid a second temporary URI and copy. */
    if (_fn_legacy_appkey_build_uri(key_id, (char *)transfer + 3,
                                    (uint16_t)(transfer_size - 3)) != 31) return false;
    transfer[0] = 1;
    transfer[1] = 31;
    transfer[2] = 0;
    transfer[34] = transfer[35] = transfer[36] = transfer[37] = 0;
    transfer[38] = (uint8_t)capacity;
    transfer[39] = (uint8_t)(capacity >> 8);
    if (fn_raw_call(FN_DEVICE_FILE, FN_CMD_FILE_READ, transfer, 40, transfer,
                    (uint16_t)(FN_LEGACY_APPKEY_READ_RESPONSE_HEADER + capacity), &raw) != FN_OK ||
        raw.status != FN_OK || raw.payload_length < FN_LEGACY_APPKEY_READ_RESPONSE_HEADER ||
        transfer[0] != 1 || FN_GET_LE32(&transfer[4]) != 0) return false;
    data_len = FN_GET_LE16(&transfer[8]);
    if ((uint16_t)(FN_LEGACY_APPKEY_READ_RESPONSE_HEADER + data_len) > raw.payload_length ||
        data_len > capacity) return false;
    if (data_len) memcpy(data, transfer + FN_LEGACY_APPKEY_READ_RESPONSE_HEADER, data_len);
    *count = data_len;
    return true;
}
