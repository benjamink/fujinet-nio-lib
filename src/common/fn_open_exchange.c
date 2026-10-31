#include <string.h>

#include "fn_internal.h"
#include "fn_platform.h"

/* Shared by fn_open() and fn_open_translated(): send the request already built in
   _fn_req_buf, parse the Open reply and record the session slot. */
uint8_t fn_open_exchange(fn_handle_t *handle, uint16_t req_len)
{
    uint8_t result;
    int8_t slot;
    fn_session_t *session;

    /* Reserve a local record before sending Open.  The common implementation
       is foreground/non-reentrant, so this slot remains ours through the
       exchange.  This prevents a successful remote Open from exposing a
       handle that the local library cannot own or later close. */
    *handle = FN_INVALID_HANDLE;
    slot = fn_find_free_slot();
    if (slot < 0) {
        return FN_ERR_NO_HANDLES;
    }

    session = &_fn_sessions[slot];
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
    result = fn_parse_open_response();
    if (result != FN_OK) {
        return result;
    }

    *handle = _fn_parse_ctx.handle;
    session->active = 1;
    session->handle = _fn_parse_ctx.handle;
    session->proto_flags = _fn_parse_ctx.proto_flags;
    session->needs_body = 0;
    session->write_offset = 0;
    session->read_offset = 0;

    if (_fn_parse_ctx.flags & FN_OPEN_RESP_NEEDS_BODY) {
        session->needs_body = 1;
    }

    return FN_OK;
}
