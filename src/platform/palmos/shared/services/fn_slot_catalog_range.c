#include "../service.h"

SERVICE(NioLibSlotCatalogRange, (UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 lower, UInt16 upper, UInt16 cursor, UInt16 request_flags, UInt16 max_uri_bytes, UInt16 max_payload_bytes, fn_slot_catalog_page_t * out, UInt16 *resultP),
        (refNum, token, io, lower, upper, cursor, request_flags, max_uri_bytes, max_payload_bytes, out, resultP))
{
    Err err;
    if (!resultP || lower > 255 || upper > 255 || cursor > 255 || request_flags > 255 || max_uri_bytes > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_slot_catalog_range(io, lower, upper, cursor, request_flags, max_uri_bytes, max_payload_bytes, out);
    }
    nl_leave();
    return errNone;
}
