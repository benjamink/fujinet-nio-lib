#include "../service.h"

SERVICE(NioLibFormatMountDisplay, (UInt16 refNum, UInt32 token, fn_mount_resolve_io_t * io, const char * canonical_uri, char * display_path, UInt16 display_cap, uint8_t * flags_out, UInt16 *resultP),
        (refNum, token, io, canonical_uri, display_path, display_cap, flags_out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_format_mount_display(io, canonical_uri, display_path, display_cap, flags_out);
    }
    nl_leave();
    return errNone;
}
