#include "../service.h"

SERVICE(NioLibSlotCatalogPut, (UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, UInt16 flags, const char * target, fn_slot_catalog_entry_t * out, UInt16 *resultP),
        (refNum, token, io, index, flags, target, out, resultP))
{
    Err err;
    if (!resultP || index > 255 || flags > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_slot_catalog_put(io, index, flags, target, out);
    }
    nl_leave();
    return errNone;
}
