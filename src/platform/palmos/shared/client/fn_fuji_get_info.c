#include "../client_private.h"

uint8_t fn_fuji_get_info(fn_fuji_info_t * info)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFujiGetInfo(nl_client_ref, nl_client_token,
        info, &nl_client_result_code));
}
