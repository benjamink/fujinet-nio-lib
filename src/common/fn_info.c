#include "fn_internal.h"
#include "fn_platform.h"

uint8_t fn_info(fn_handle_t handle,
                uint16_t *http_status,
                uint32_t *content_length,
                uint8_t *flags)
{
    uint16_t req_len;
    uint8_t result;
    int8_t slot;

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == FN_INVALID_HANDLE) {
        return FN_ERR_INVALID;
    }

    slot = fn_find_session(handle);
    if (slot < 0) {
        return FN_ERR_NOT_FOUND;
    }

    req_len = fn_build_info_packet(_fn_req_buf, handle);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    _fn_transport_ctx.request = _fn_req_buf;
    _fn_transport_ctx.req_len = req_len;
    _fn_transport_ctx.response = _fn_resp_buf;
    _fn_transport_ctx.resp_max = FN_MAX_PACKET_SIZE;

    result = fn_transport_exchange();
    if (result != FN_OK) {
        return result;
    }

    _fn_parse_ctx.response = _fn_resp_buf;
    _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
    result = fn_parse_info_response();
    if (result != FN_OK) {
        return result;
    }

    if (http_status != NULL) {
        *http_status = _fn_parse_ctx.http_status;
    }
    if (content_length != NULL) {
        *content_length = _fn_parse_ctx.content_length;
    }
    if (flags != NULL) {
        *flags = _fn_parse_ctx.flags;
    }

    return FN_OK;
}
