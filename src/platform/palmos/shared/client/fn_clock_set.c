#include "../client_private.h"
uint8_t fn_clock_set(const FN_TIME_T *time)
{
    UInt8 bytes[8];
    UInt16 i;
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    if (!time) return FN_ERR_INVALID;
    for (i = 0; i < 8; ++i) bytes[i] = (UInt8)(*time >> (8 * i));
    return nl_client_translate(NioLibClockSet(nl_client_ref, nl_client_token,
                                bytes, &nl_client_result_code));
}
