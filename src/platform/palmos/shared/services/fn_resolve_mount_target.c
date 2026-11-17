#include "../service.h"

SERVICE(NioLibResolveMountTarget, (UInt16 refNum, UInt32 token, fn_mount_resolve_io_t * io, const char * host, const char * browse_path, const char * leaf, char * canonical_uri, UInt16 canonical_cap, char * display_path, UInt16 display_cap, uint8_t * flags_out, UInt16 *resultP),
        (refNum, token, io, host, browse_path, leaf, canonical_uri, canonical_cap, display_path, display_cap, flags_out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_resolve_mount_target(io, host, browse_path, leaf, canonical_uri, canonical_cap, display_path, display_cap, flags_out);
    }
    nl_leave();
    return errNone;
}
