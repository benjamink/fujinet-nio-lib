#include "../service.h"

SERVICE(NioLibAppstoreRead, (UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, UInt32 offset, uint8_t * buf, UInt16 max_len, fn_appstore_read_t * out, UInt16 *resultP),
        (refNum, token, io, namespace_name, key, offset, buf, max_len, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_appstore_read(io, namespace_name, key, offset, buf, max_len, out);
    }
    nl_leave();
    return errNone;
}
