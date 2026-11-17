#include "fn_internal.h"

uint8_t fn_clock_copy_format_response(uint8_t *time_data, uint16_t data_offset,
                                      uint16_t data_len, uint8_t format)
{
    uint16_t i, count, capacity = fn_clock_format_capacity(format);
    uint8_t string_format = format == FN_TIME_FORMAT_TZ_ISO || format == FN_TIME_FORMAT_UTC_ISO;
    if (!capacity || data_len < 2) return FN_ERR_INVALID;
    if (fn_clock_response_byte(data_offset) != FN_CLOCK_VERSION) return FN_ERR_UNSUPPORTED;
    if (fn_clock_response_byte(data_offset + 1) != format) return FN_ERR_IO;
    count = (uint16_t)(data_len - 2);
    if (string_format) {
        /* The current host sends ISO bytes without a trailing NUL. Accept a
         * final NUL from older peers, and always terminate within caller space. */
        if (count && !fn_clock_response_byte(data_offset + 1 + count)) --count;
        if (!count || count >= capacity) return FN_ERR_IO;
        for (i = 0; i < count; ++i)
            if (!fn_clock_response_byte(data_offset + 2 + i)) return FN_ERR_IO;
    } else if (count != capacity) return FN_ERR_IO;
    for (i = 0; i < count; ++i)
        time_data[i] = fn_clock_response_byte(data_offset + 2 + i);
    if (string_format) time_data[count] = 0;
    return FN_OK;
}
