#include "../client_private.h"

uint8_t fn_slot_catalog_put(fn_slot_catalog_io_t * io, uint8_t index, uint8_t flags, const char * target, fn_slot_catalog_entry_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibSlotCatalogPut(nl_client_ref, nl_client_token,
        io, index, flags, target, out, &nl_client_result_code));
}
