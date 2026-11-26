#include "../client_private.h"

uint8_t fn_wifi_try_join(const char * ssid, const char * password)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiTryJoin(nl_client_ref, nl_client_token,
        ssid, password, &nl_client_result_code));
}
