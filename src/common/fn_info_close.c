#include "fn_internal.h"
#include "fn_platform.h"

uint8_t fn_close(fn_handle_t handle)
{
    uint16_t req_len;
    uint8_t result;

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == FN_INVALID_HANDLE) {
        return FN_ERR_INVALID;
    }

    req_len = fn_build_close_packet(_fn_req_buf, handle);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    _fn_transport_ctx.request = _fn_req_buf;
    _fn_transport_ctx.req_len = req_len;
    _fn_transport_ctx.response = _fn_resp_buf;
    _fn_transport_ctx.resp_max = FN_MAX_PACKET_SIZE;

    result = fn_transport_exchange();

    fn_free_handle(handle);

    return result;
}
