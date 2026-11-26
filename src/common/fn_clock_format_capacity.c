#include "fujinet-nio.h"
uint16_t fn_clock_format_capacity(uint8_t format)
{
    switch (format) {
        case FN_TIME_FORMAT_SIMPLE: return 7;
        case FN_TIME_FORMAT_PRODOS: return 4;
        case FN_TIME_FORMAT_APETIME: return 6;
        case FN_TIME_FORMAT_TZ_ISO:
        case FN_TIME_FORMAT_UTC_ISO: return FN_MAX_TIME_STRING;
        case FN_TIME_FORMAT_APPLE3_SOS: return 16;
        default: return 0;
    }
}
