#include "../client_private.h"
#include "fn_internal.h"
uint8_t fn_clock_get_format(uint8_t *time_data, FnTimeFormat format)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    if ((unsigned)format > FN_TIME_FORMAT_APPLE3_SOS) return FN_ERR_INVALID;
    return nl_client_translate(NioLibClockGetFormat(nl_client_ref, nl_client_token,
                time_data, (UInt16)format, fn_clock_format_capacity((uint8_t)format), &nl_client_result_code));
}
