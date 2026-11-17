#include "../client_private.h"

uint8_t fn_file_read(fn_file_io_t * io, const char * uri, uint32_t offset, uint8_t * data, uint16_t capacity, fn_file_read_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFileRead(nl_client_ref, nl_client_token,
        io, uri, offset, data, capacity, out, &nl_client_result_code));
}
