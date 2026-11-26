#include "../client_private.h"

uint8_t fn_clock_set_timezone_save(const char * tz)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibClockSetTimezoneSave(nl_client_ref, nl_client_token,
        tz, &nl_client_result_code));
}
