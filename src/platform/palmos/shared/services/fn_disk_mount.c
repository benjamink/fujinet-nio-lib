#include "../service.h"

SERVICE(NioLibDiskMount, (UInt16 refNum, UInt32 token, UInt16 slot, const char * uri, UInt16 readonly, UInt16 type, UInt16 sector_size_hint, fn_disk_info_t * info, UInt16 *resultP),
        (refNum, token, slot, uri, readonly, type, sector_size_hint, info, resultP))
{
    Err err;
    if (!resultP || slot > 255 || readonly > 255 || type > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_disk_mount(slot, uri, readonly, type, sector_size_hint, info);
    }
    nl_leave();
    return errNone;
}
