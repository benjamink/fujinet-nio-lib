#include "../service.h"

SERVICE(NioLibClockSyncNetworkTime, (UInt16 refNum, UInt32 token, UInt8 * timeLE8, UInt16 *resultP),
        (refNum, token, timeLE8, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    FN_TIME_T time;
    UInt16 i;
    if (!timeLE8) { nl_leave(); return nioLibErrParam; }
    *resultP = fn_clock_sync_network_time(&time);
    if (!*resultP) for (i = 0; i < 8; ++i) { timeLE8[i] = (UInt8)time; time >>= 8; }
    }
    nl_leave();
    return errNone;
}
