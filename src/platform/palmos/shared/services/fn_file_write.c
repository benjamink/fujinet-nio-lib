#include "../service.h"

SERVICE(NioLibFileWrite, (UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt32 offset, const uint8_t * data, UInt16 length, fn_file_write_t * out, UInt16 *resultP),
        (refNum, token, io, uri, offset, data, length, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    nl_clear_last_result();
    *resultP = fn_file_write(io, uri, offset, data, length, out);
    }
    nl_note_mutation_failure(*resultP);
    nl_leave();
    return errNone;
}
