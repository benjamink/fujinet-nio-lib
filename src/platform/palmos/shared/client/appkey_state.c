#include "../client_private.h"
/* Namespace belongs to the application, so details may be set before fn_init.
 * Reapply it to the currently owned library session before a key operation. */
UInt16 nl_client_key_creator, nl_client_key_app, nl_client_key_size;
uint8_t nl_client_appkey_ready(void)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibLegacySetAppkeyDetails(nl_client_ref,
        nl_client_token, nl_client_key_creator, nl_client_key_app,
        nl_client_key_size, &nl_client_result_code));
}
