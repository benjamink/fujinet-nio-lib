#include "../client_private.h"

uint8_t fn_appstore_stat(fn_appstore_io_t * io, const char * namespace_name, const char * key, fn_appstore_stat_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibAppstoreStat(nl_client_ref, nl_client_token,
        io, namespace_name, key, out, &nl_client_result_code));
}
