#include "../service.h"

SERVICE(NioLibSlotCatalogDelete, (UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, uint8_t * deleted, UInt16 *resultP),
        (refNum, token, io, index, deleted, resultP))
{
    Err err;
    if (!resultP || index > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_slot_catalog_delete(io, index, deleted);
    }
    nl_leave();
    return errNone;
}
