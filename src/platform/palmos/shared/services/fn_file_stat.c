#include "../service.h"

SERVICE(NioLibFileStat, (UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, fn_file_stat_t * out, UInt16 *resultP),
        (refNum, token, io, uri, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_file_stat(io, uri, out);
    }
    nl_leave();
    return errNone;
}
