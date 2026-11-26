#include "../client_private.h"

uint8_t fn_file_mkdir(fn_file_io_t * io, const char * uri, uint8_t flags)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFileMkdir(nl_client_ref, nl_client_token,
        io, uri, flags, &nl_client_result_code));
}
