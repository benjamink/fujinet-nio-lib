#include "../client_private.h"

uint8_t fn_slot_catalog_delete(fn_slot_catalog_io_t * io, uint8_t index, uint8_t * deleted)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibSlotCatalogDelete(nl_client_ref, nl_client_token,
        io, index, deleted, &nl_client_result_code));
}
