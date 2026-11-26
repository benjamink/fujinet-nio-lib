#include "../client_private.h"

uint8_t fn_wifi_get_join_status(uint8_t * state)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiGetJoinStatus(nl_client_ref, nl_client_token,
        state, &nl_client_result_code));
}
