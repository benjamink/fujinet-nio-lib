#include "../service.h"

SERVICE(NioLibFujiGetInfo, (UInt16 refNum, UInt32 token, fn_fuji_info_t * info, UInt16 *resultP),
        (refNum, token, info, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_fuji_get_info(info);
    }
    nl_leave();
    return errNone;
}
