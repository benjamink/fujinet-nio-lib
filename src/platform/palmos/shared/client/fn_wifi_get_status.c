#include "../client_private.h"

uint8_t fn_wifi_get_status(fn_wifi_status_t * status)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiGetStatus(nl_client_ref, nl_client_token,
        status, &nl_client_result_code));
}
