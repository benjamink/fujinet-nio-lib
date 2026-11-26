#include "../service.h"

SERVICE(NioLibClockSet, (UInt16 refNum, UInt32 token, const UInt8 * timeLE8, UInt16 *resultP),
        (refNum, token, timeLE8, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    FN_TIME_T time = 0;
    UInt16 i;
    if (!timeLE8) { nl_leave(); return nioLibErrParam; }
    for (i = 0; i < 8; ++i) time |= ((FN_TIME_T)timeLE8[i]) << (8 * i);
    *resultP = fn_clock_set(&time);
    }
    nl_leave();
    return errNone;
}
