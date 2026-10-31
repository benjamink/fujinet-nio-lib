#include <string.h>

#include "fn_internal.h"

uint8_t fn_parse_read_response(void)
{
    uint8_t result;
    uint16_t actual_data_len;
    const uint8_t *response = _fn_parse_ctx.response;

    result = fn_parse_response_header();
    if (result != FN_OK) {
        return result;
    }
    if (_fn_parse_ctx.status != FN_OK) {
        return _fn_parse_ctx.status;
    }
    if (_fn_parse_ctx.data_len < 12) {
        return FN_ERR_INVALID;
    }

    response += _fn_parse_ctx.data_offset;
    _fn_parse_ctx.flags = response[1];
    /* The reserved u16 carries the bytes the device still holds, when flag bit3 says so. */
    _fn_parse_ctx.available = (_fn_parse_ctx.flags & FN_READ_AVAILABLE) != 0
                                  ? FN_GET_LE16(response + 2)
                                  : 0;
    _fn_parse_ctx.handle = FN_GET_LE16(response + 4);
    _fn_parse_ctx.offset_echo = FN_GET_LE32(response + 6);
    actual_data_len = FN_GET_LE16(response + 10);
    if (actual_data_len > (uint16_t)(_fn_parse_ctx.data_len - 12) ||
        actual_data_len > _fn_parse_ctx.data_max) {
        return FN_ERR_INVALID;
    }

    if (actual_data_len > 0 && _fn_parse_ctx.data != NULL) {
        memcpy(_fn_parse_ctx.data, response + 12, actual_data_len);
    }

    _fn_parse_ctx.data_len = actual_data_len;
    return FN_OK;
}
