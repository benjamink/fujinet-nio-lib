#include "../client_private.h"

uint8_t fn_disk_mount(uint8_t slot, const char * uri, uint8_t readonly, uint8_t type, uint16_t sector_size_hint, fn_disk_info_t * info)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibDiskMount(nl_client_ref, nl_client_token,
        slot, uri, readonly, type, sector_size_hint, info, &nl_client_result_code));
}
