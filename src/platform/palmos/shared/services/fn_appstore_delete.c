#include "../service.h"

SERVICE(NioLibAppstoreDelete, (UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, fn_appstore_delete_t * out, UInt16 *resultP),
        (refNum, token, io, namespace_name, key, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_appstore_delete(io, namespace_name, key, out);
    }
    nl_leave();
    return errNone;
}
