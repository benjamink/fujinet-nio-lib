#include "../service.h"

SERVICE(NioLibFileRead, (UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt32 offset, uint8_t * data, UInt16 capacity, fn_file_read_t * out, UInt16 *resultP),
        (refNum, token, io, uri, offset, data, capacity, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_file_read(io, uri, offset, data, capacity, out);
    }
    nl_leave();
    return errNone;
}
