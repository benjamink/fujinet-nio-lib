#include "../service.h"

SERVICE(NioLibAppstoreWrite, (UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, UInt32 offset, const uint8_t * data, UInt16 len, fn_appstore_write_t * out, UInt16 *resultP),
        (refNum, token, io, namespace_name, key, offset, data, len, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_appstore_write(io, namespace_name, key, offset, data, len, out);
    }
    nl_leave();
    return errNone;
}
