#include "../service.h"
#include "fn_internal.h"

SERVICE(NioLibClockGetTz, (UInt16 refNum, UInt32 token, UInt8 * time_data, const Char * tz, UInt16 format, UInt16 capacity, UInt16 *resultP),
        (refNum, token, time_data, tz, format, capacity, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    if (!time_data || capacity < fn_clock_format_capacity((uint8_t)format) || format > FN_TIME_FORMAT_APPLE3_SOS)
        { nl_leave(); return nioLibErrParam; }
    *resultP = fn_clock_get_tz(time_data, tz, (FnTimeFormat)format);
    }
    nl_leave();
    return errNone;
}
