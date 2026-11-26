#include "../service.h"

SERVICE(NioLibAppstoreStat, (UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, fn_appstore_stat_t * out, UInt16 *resultP),
        (refNum, token, io, namespace_name, key, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_appstore_stat(io, namespace_name, key, out);
    }
    nl_leave();
    return errNone;
}
