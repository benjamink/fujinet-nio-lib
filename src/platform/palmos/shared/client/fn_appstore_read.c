#include "../client_private.h"

uint8_t fn_appstore_read(fn_appstore_io_t * io, const char * namespace_name, const char * key, uint32_t offset, uint8_t * buf, uint16_t max_len, fn_appstore_read_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibAppstoreRead(nl_client_ref, nl_client_token,
        io, namespace_name, key, offset, buf, max_len, out, &nl_client_result_code));
}
