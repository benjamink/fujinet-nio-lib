#include "../service.h"

SERVICE(NioLibSlotCatalogGet, (UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, fn_slot_catalog_entry_t * out, UInt16 *resultP),
        (refNum, token, io, index, out, resultP))
{
    Err err;
    if (!resultP || index > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_slot_catalog_get(io, index, out);
    }
    nl_leave();
    return errNone;
}
