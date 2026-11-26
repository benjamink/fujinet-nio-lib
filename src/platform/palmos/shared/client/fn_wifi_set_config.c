#include "../client_private.h"

uint8_t fn_wifi_set_config(const fn_wifi_config_update_t * update)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiSetConfig(nl_client_ref, nl_client_token,
        update, &nl_client_result_code));
}
