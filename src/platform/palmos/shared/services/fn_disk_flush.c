#include "../service.h"

SERVICE(NioLibDiskFlush, (UInt16 refNum, UInt32 token, UInt16 slot, UInt16 *resultP),
        (refNum, token, slot, resultP))
{
    Err err;
    if (!resultP || slot > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_disk_flush(slot);
    }
    nl_leave();
    return errNone;
}
