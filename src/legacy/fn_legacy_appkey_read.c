#include <string.h>

#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_protocol.h"
#include "fn_raw.h"
#include "fn_legacy_appkey_internal.h"

bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data)
{
    return fuji_read_appkey_buffered(key_id, count, data, _fn_legacy_appkey_transfer,
                                     FN_LEGACY_APPKEY_TRANSFER_MAX);
}
