#include "../client_private.h"

uint8_t fn_slot_catalog_range(fn_slot_catalog_io_t * io, uint8_t lower, uint8_t upper, uint8_t cursor, uint8_t request_flags, uint8_t max_uri_bytes, uint16_t max_payload_bytes, fn_slot_catalog_page_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibSlotCatalogRange(nl_client_ref, nl_client_token,
        io, lower, upper, cursor, request_flags, max_uri_bytes, max_payload_bytes, out, &nl_client_result_code));
}
