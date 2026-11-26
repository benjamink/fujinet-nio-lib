#include "../client_private.h"
uint8_t fn_disk_inspect(fn_service_io_t *io, const char *uri, uint8_t type,
                        uint16_t sector_size_hint, fn_disk_inspection_t *out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibDiskInspect(nl_client_ref, nl_client_token,
        io, uri, type, sector_size_hint, out, &nl_client_result_code));
}
