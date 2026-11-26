#include "../client_private.h"
#include "fn_legacy_appkey.h"
bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data)
{
    if (nl_client_appkey_ready()) return false;
    return nl_client_translate(NioLibLegacyReadAppkey(nl_client_ref, nl_client_token,
                         key_id, count, data, &nl_client_result_code)) == FN_OK;
}
