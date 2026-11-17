#include "../service.h"

SERVICE(NioLibDiskLastError, (UInt16 refNum, UInt32 token, UInt16 * errorP, UInt16 *resultP),
        (refNum, token, errorP, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    if (!errorP) { nl_leave(); return nioLibErrParam; }
    *errorP = fn_disk_last_error();
    *resultP = FN_OK;
    }
    nl_leave();
    return errNone;
}
