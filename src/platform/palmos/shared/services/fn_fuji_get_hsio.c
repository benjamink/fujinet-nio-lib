#include "../service.h"
SERVICE(NioLibFujiGetHsio, (UInt16 refNum, UInt32 token, uint8_t *index, UInt16 *resultP),
        (refNum, token, index, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    *resultP = fn_fuji_get_hsio(index);
    nl_leave();
    return errNone;
}
