#include "../client_private.h"
uint8_t fn_clock_sync_network_time(FN_TIME_T *time)
{
    UInt8 bytes[8];
    UInt16 i;
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    if (!time) return FN_ERR_INVALID;
    result = nl_client_translate(NioLibClockSyncNetworkTime(nl_client_ref,
                                nl_client_token, bytes, &nl_client_result_code));
    if (!result) {
        *time = 0;
        for (i = 0; i < 8; ++i) *time |= ((FN_TIME_T)bytes[i]) << (8 * i);
    }
    return result;
}
