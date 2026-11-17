#include "../client_private.h"
#include "fn_legacy_appkey.h"
bool fuji_read_appkey_buffered(uint8_t key_id, uint16_t *count, uint8_t *data, uint8_t *scratch, uint16_t scratch_size)
{
    if (nl_client_appkey_ready()) return false;
    return nl_client_translate(NioLibLegacyReadAppkeyBuffered(nl_client_ref, nl_client_token,
                         key_id, count, data, scratch, scratch_size, &nl_client_result_code)) == FN_OK;
}
