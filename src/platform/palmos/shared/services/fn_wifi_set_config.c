#include "../service.h"

SERVICE(NioLibWifiSetConfig, (UInt16 refNum, UInt32 token, const fn_wifi_config_update_t * update, UInt16 *resultP),
        (refNum, token, update, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_set_config(update);
    }
    nl_leave();
    return errNone;
}
