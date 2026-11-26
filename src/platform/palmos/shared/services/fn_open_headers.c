#include "../service.h"

SERVICE(NioLibOpenHeaders, (UInt16 refNum, UInt32 token, fn_handle_t * handle, UInt16 method, const char * url, UInt16 flags, const fn_http_header_t * headers, UInt16 header_count, UInt32 body_len, UInt16 *resultP),
        (refNum, token, handle, method, url, flags, headers, header_count, body_len, resultP))
{
    Err err;
    if (!resultP || method > 255 || flags > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    nl_clear_last_result();
    *resultP = fn_open_headers(handle, method, url, flags, headers, header_count, body_len);
    }
    nl_note_mutation_failure(*resultP);
    nl_leave();
    return errNone;
}
