#include "../service.h"

SERVICE(NioLibFileListPages, (UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, const fn_list_pages_req_t * req, fn_list_pages_t * out, UInt16 *resultP),
        (refNum, token, io, uri, req, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_file_list_pages(io, uri, req, out);
    }
    nl_leave();
    return errNone;
}
