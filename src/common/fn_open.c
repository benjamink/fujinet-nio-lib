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

/* Shared by fn_open() and fn_open_translated(): check the arguments and map the
   public FN_OPEN_* flags to the wire flags. Keep every open-flag mapping here so
   the two calls cannot drift apart. */
uint8_t fn_open_prepare(const fn_handle_t *handle,
                        const char *url,
                        uint8_t flags,
                        uint8_t *wire_flags)
{
    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == NULL || url == NULL) {
        return FN_ERR_INVALID;
    }

    if (strlen(url) > FN_MAX_URL_LEN) {
        return FN_ERR_URL_TOO_LONG;
    }

    /* These public bits deliberately mirror the protocol bits. Assert that
       contract so a future flag change cannot silently alter the wire. */
#if FN_OPEN_FOLLOW_REDIR != FN_OPEN_FLAG_FOLLOW_REDIR || \
    FN_OPEN_BODY_UNKNOWN != FN_OPEN_FLAG_BODY_UNKNOWN || \
    FN_OPEN_ALLOW_EVICT != FN_OPEN_FLAG_ALLOW_EVICT || \
    FN_OPEN_STREAM_NO_PROBE != FN_OPEN_FLAG_STREAM_NO_PROBE || \
    FN_OPEN_KEEP != FN_OPEN_FLAG_KEEP
#error Public and wire Open flag values must be mapped explicitly
#endif
    *wire_flags = flags & (FN_OPEN_FOLLOW_REDIR | FN_OPEN_BODY_UNKNOWN |
                           FN_OPEN_ALLOW_EVICT | FN_OPEN_STREAM_NO_PROBE | FN_OPEN_KEEP);

    return FN_OK;
}

uint8_t fn_open(fn_handle_t *handle,
                uint8_t method,
                const char *url,
                uint8_t flags)
{
    uint16_t req_len;
    uint8_t open_flags;
    uint8_t result;

    result = fn_open_prepare(handle, url, flags, &open_flags);
    if (result != FN_OK) {
        return result;
    }

    req_len = fn_build_open_packet(_fn_req_buf, method, open_flags, url);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    return fn_open_exchange(handle, req_len);
}
