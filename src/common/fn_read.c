#include "fn_internal.h"
#include "fn_platform.h"

static uint16_t _fn_last_read_available;

uint8_t fn_read(fn_handle_t handle,
                uint32_t offset,
                uint8_t *buf,
                uint16_t max_len,
                uint16_t *bytes_read,
                uint8_t *flags)
{
    uint16_t req_len;
    uint16_t chunk;
#ifndef __CC65__
    uint16_t total;
#endif
    uint8_t result;
    int8_t slot;
    fn_session_t *session;

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == FN_INVALID_HANDLE || buf == NULL || bytes_read == NULL) {
        return FN_ERR_INVALID;
    }

    *bytes_read = 0;
    if (flags != NULL) {
        *flags = 0;
    }

    slot = fn_find_session(handle);
    if (slot < 0) {
        return FN_ERR_NOT_FOUND;
    }
    session = &_fn_sessions[slot];

    if ((session->proto_flags & FN_PROTO_FLAG_SEQUENTIAL_READ) &&
        offset != session->read_offset) {
        return FN_ERR_INVALID;
    }

#ifndef __CC65__
    total = 0;
    do {
        chunk = (uint16_t)(max_len - total);
#else
    chunk = max_len;
#endif
    /* FujiBus header, maximal status field and 12-byte Network Read prefix. */
    if (chunk > FN_MAX_PACKET_SIZE - 22u) chunk = FN_MAX_PACKET_SIZE - 22u;
    req_len = fn_build_read_packet(_fn_req_buf, handle, offset, chunk);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    _fn_transport_ctx.request = _fn_req_buf;
    _fn_transport_ctx.req_len = req_len;
    _fn_transport_ctx.response = _fn_resp_buf;
    _fn_transport_ctx.resp_max = FN_MAX_PACKET_SIZE;

    result = fn_transport_exchange();
    if (result != FN_OK) {
#ifndef __CC65__
        if (total && (result == FN_ERR_NOT_READY || result == FN_ERR_BUSY)) return FN_OK;
#endif
        return result;
    }

    _fn_parse_ctx.response = _fn_resp_buf;
    _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
    _fn_parse_ctx.data = buf;
    _fn_parse_ctx.data_max = chunk;
    result = fn_parse_read_response();
    if (result != FN_OK) {
#ifndef __CC65__
        if (total && (result == FN_ERR_NOT_READY || result == FN_ERR_BUSY)) return FN_OK;
#endif
        return result;
    }

    _fn_last_read_available = _fn_parse_ctx.available;
    if (flags != NULL) {
        *flags = _fn_parse_ctx.flags;
    }
#ifndef __CC65__
    total = (uint16_t)(total + _fn_parse_ctx.data_len);
    *bytes_read = total;
#else
    *bytes_read = _fn_parse_ctx.data_len;
#endif

    if ((session->proto_flags & FN_PROTO_FLAG_SEQUENTIAL_READ) && _fn_parse_ctx.data_len > 0) {
        session->read_offset += _fn_parse_ctx.data_len;
    }

#ifndef __CC65__
    if (_fn_parse_ctx.data_len < chunk || (_fn_parse_ctx.flags & FN_READ_EOF) ||
        total == max_len || _fn_parse_ctx.data_len == 0) break;
    if (offset > UINT32_MAX - _fn_parse_ctx.data_len) return FN_ERR_INVALID;
    offset += _fn_parse_ctx.data_len;
    buf += _fn_parse_ctx.data_len;
    } while (total < max_len);
#endif

    return FN_OK;
}

uint16_t fn_read_available(void)
{
    return _fn_last_read_available;
}
