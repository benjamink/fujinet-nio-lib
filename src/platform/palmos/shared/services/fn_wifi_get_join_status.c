#include "../service.h"

SERVICE(NioLibWifiGetJoinStatus, (UInt16 refNum, UInt32 token, uint8_t * state, UInt16 *resultP),
        (refNum, token, state, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_get_join_status(state);
    }
    nl_leave();
    return errNone;
}
