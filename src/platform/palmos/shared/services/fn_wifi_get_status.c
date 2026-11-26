#include "../service.h"

SERVICE(NioLibWifiGetStatus, (UInt16 refNum, UInt32 token, fn_wifi_status_t * status, UInt16 *resultP),
        (refNum, token, status, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_get_status(status);
    }
    nl_leave();
    return errNone;
}
