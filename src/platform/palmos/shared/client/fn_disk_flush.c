#include "../client_private.h"

uint8_t fn_disk_flush(uint8_t slot)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibDiskFlush(nl_client_ref, nl_client_token,
        slot, &nl_client_result_code));
}
