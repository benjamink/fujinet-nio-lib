#include "../service.h"

SERVICE(NioLibWifiGetAdapterInfo, (UInt16 refNum, UInt32 token, fn_wifi_adapter_info_t * info, UInt16 *resultP),
        (refNum, token, info, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_get_adapter_info(info);
    }
    nl_leave();
    return errNone;
}
