#include "../client_private.h"

uint8_t fn_file_stat(fn_file_io_t * io, const char * uri, fn_file_stat_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFileStat(nl_client_ref, nl_client_token,
        io, uri, out, &nl_client_result_code));
}
