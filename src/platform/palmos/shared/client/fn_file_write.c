#include "../client_private.h"

uint8_t fn_file_write(fn_file_io_t * io, const char * uri, uint32_t offset, const uint8_t * data, uint16_t length, fn_file_write_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFileWrite(nl_client_ref, nl_client_token,
        io, uri, offset, data, length, out, &nl_client_result_code));
}
