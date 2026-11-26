#include "../client_private.h"
uint8_t fn_disk_last_error(void)
{
    UInt16 detail = FN_DISK_ERR_UNREPORTED;
    if (nl_client_extended_ready()) return FN_DISK_ERR_UNREPORTED;
    if (nl_client_translate(NioLibDiskLastError(nl_client_ref, nl_client_token,
                              &detail, &nl_client_result_code))) return FN_DISK_ERR_UNREPORTED;
    return (uint8_t)detail;
}
