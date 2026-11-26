#include "../client_private.h"

uint8_t fn_appstore_write(fn_appstore_io_t * io, const char * namespace_name, const char * key, uint32_t offset, const uint8_t * data, uint16_t len, fn_appstore_write_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibAppstoreWrite(nl_client_ref, nl_client_token,
        io, namespace_name, key, offset, data, len, out, &nl_client_result_code));
}
