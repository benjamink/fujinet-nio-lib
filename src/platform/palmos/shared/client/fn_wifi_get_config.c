#include "../client_private.h"

uint8_t fn_wifi_get_config(fn_wifi_config_t * config)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiGetConfig(nl_client_ref, nl_client_token,
        config, &nl_client_result_code));
}
