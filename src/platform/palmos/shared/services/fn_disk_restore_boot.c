#include "../service.h"

SERVICE(NioLibDiskRestoreBoot, (UInt16 refNum, UInt32 token, UInt16 slot, fn_disk_info_t * info, UInt16 *resultP),
        (refNum, token, slot, info, resultP))
{
    Err err;
    if (!resultP || slot > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_disk_restore_boot(slot, info);
    }
    nl_leave();
    return errNone;
}
