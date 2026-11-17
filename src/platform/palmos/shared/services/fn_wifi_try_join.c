#include "../service.h"

SERVICE(NioLibWifiTryJoin, (UInt16 refNum, UInt32 token, const char * ssid, const char * password, UInt16 *resultP),
        (refNum, token, ssid, password, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_try_join(ssid, password);
    }
    nl_leave();
    return errNone;
}
