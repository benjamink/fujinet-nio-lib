#include "../client_private.h"

uint8_t fn_format_mount_display(fn_mount_resolve_io_t * io, const char * canonical_uri, char * display_path, uint16_t display_cap, uint8_t * flags_out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFormatMountDisplay(nl_client_ref, nl_client_token,
        io, canonical_uri, display_path, display_cap, flags_out, &nl_client_result_code));
}
