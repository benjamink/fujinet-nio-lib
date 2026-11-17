#include "../service.h"
SERVICE(NioLibFujiSetHsio, (UInt16 refNum, UInt32 token, UInt16 index, UInt16 *resultP),
        (refNum, token, index, resultP))
{
    Err err;
    if (!resultP || index > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    *resultP = fn_fuji_set_hsio(index);
    nl_leave();
    return errNone;
}
