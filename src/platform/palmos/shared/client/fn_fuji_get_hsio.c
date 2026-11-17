#include "../client_private.h"
uint8_t fn_fuji_get_hsio(uint8_t * index)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFujiGetHsio(nl_client_ref, nl_client_token,
                                   index, &nl_client_result_code));
}
