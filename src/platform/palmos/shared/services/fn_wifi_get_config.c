#include "../service.h"

SERVICE(NioLibWifiGetConfig, (UInt16 refNum, UInt32 token, fn_wifi_config_t * config, UInt16 *resultP),
        (refNum, token, config, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_get_config(config);
    }
    nl_leave();
    return errNone;
}
