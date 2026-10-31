#include "fn_internal.h"

uint8_t fn_parse_open_response(void)
{
    uint8_t result;
    const uint8_t *response = _fn_parse_ctx.response;

    result = fn_parse_response_header();
    if (result != FN_OK) {
        return result;
    }
    if (_fn_parse_ctx.status != FN_OK) {
        return _fn_parse_ctx.status;
    }
    if (_fn_parse_ctx.data_len < 7) {
        return FN_ERR_INVALID;
    }

    response += _fn_parse_ctx.data_offset;
    _fn_parse_ctx.flags = response[1];
    _fn_parse_ctx.handle = FN_GET_LE16(response + 4);
    _fn_parse_ctx.proto_flags = response[6];

    return FN_OK;
}
