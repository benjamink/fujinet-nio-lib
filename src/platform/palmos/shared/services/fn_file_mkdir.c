#include "../service.h"

SERVICE(NioLibFileMkdir, (UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt16 flags, UInt16 *resultP),
        (refNum, token, io, uri, flags, resultP))
{
    Err err;
    if (!resultP || flags > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_file_mkdir(io, uri, flags);
    }
    nl_leave();
    return errNone;
}
