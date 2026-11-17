#include "../service.h"
SERVICE(NioLibDiskInspect,
    (UInt16 refNum, UInt32 token, fn_service_io_t *io, const char *uri,
     UInt16 type, UInt16 hint, fn_disk_inspection_t *out, UInt16 *resultP),
    (refNum, token, io, uri, type, hint, out, resultP))
{
    Err err;
    if (!resultP || type > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    *resultP = fn_disk_inspect(io, uri, (uint8_t)type, hint, out);
    nl_leave();
    return errNone;
}
