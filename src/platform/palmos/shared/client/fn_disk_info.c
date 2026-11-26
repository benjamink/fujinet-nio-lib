#include "../client_private.h"

uint8_t fn_disk_info(uint8_t slot, fn_disk_info_t * info)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibDiskInfo(nl_client_ref, nl_client_token,
        slot, info, &nl_client_result_code));
}
