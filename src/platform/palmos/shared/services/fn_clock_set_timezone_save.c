#include "../service.h"

SERVICE(NioLibClockSetTimezoneSave, (UInt16 refNum, UInt32 token, const char * tz, UInt16 *resultP),
        (refNum, token, tz, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_clock_set_timezone_save(tz);
    }
    nl_leave();
    return errNone;
}
