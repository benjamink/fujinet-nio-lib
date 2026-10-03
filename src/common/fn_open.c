#include <string.h>

#include "fn_internal.h"
#include "fn_platform.h"

/* Shared by fn_open() and fn_open_translated(): send the request already built in
   _fn_req_buf, parse the Open reply and record the session slot. */
uint8_t fn_open_exchange(fn_handle_t *handle, uint16_t req_len)
{
    uint8_t result;
    int8_t slot;

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

    slot = fn_find_free_slot();
    if (slot < 0) {
        *handle = _fn_parse_ctx.handle;
        return FN_OK;
    }

    *handle = _fn_parse_ctx.handle;
    _fn_sessions[slot].active = 1;
    _fn_sessions[slot].handle = _fn_parse_ctx.handle;
    _fn_sessions[slot].proto_flags = _fn_parse_ctx.proto_flags;
    _fn_sessions[slot].needs_body = 0;
    _fn_sessions[slot].write_offset = 0;
    _fn_sessions[slot].read_offset = 0;

    if (_fn_parse_ctx.flags & FN_OPEN_RESP_NEEDS_BODY) {
        _fn_sessions[slot].needs_body = 1;
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
    uint8_t open_flags;

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == NULL || url == NULL) {
        return FN_ERR_INVALID;
    }

    if (strlen(url) > FN_MAX_URL_LEN) {
        return FN_ERR_URL_TOO_LONG;
    }

    open_flags = 0;
    if (flags & FN_OPEN_FOLLOW_REDIR) {
        open_flags |= FN_OPEN_FLAG_FOLLOW_REDIR;
    }
    if (flags & FN_OPEN_BODY_UNKNOWN) {
        open_flags |= FN_OPEN_FLAG_BODY_UNKNOWN;
    }
    if (flags & FN_OPEN_ALLOW_EVICT) {
        open_flags |= FN_OPEN_FLAG_ALLOW_EVICT;
    }
    if (flags & FN_OPEN_STREAM_NO_PROBE) {
        open_flags |= FN_OPEN_FLAG_STREAM_NO_PROBE;
    }

    *wire_flags = open_flags;
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

uint8_t fn_tcp_open(fn_handle_t *handle,
                    const char *host,
                    uint16_t port)
{
    uint8_t offset;
    uint8_t started;
    uint16_t p;

    strcpy(_fn_tcp_url, "tcp://");
    offset = 6;

    if (offset + strlen(host) > FN_MAX_URL_LEN - 10) {
        return FN_ERR_URL_TOO_LONG;
    }
    strcpy(_fn_tcp_url + offset, host);
    offset += (uint8_t)strlen(host);

    _fn_tcp_url[offset++] = ':';

    p = port;
    started = 0;
    if (p >= 10000) {
        _fn_tcp_url[offset++] = (char)('0' + (p / 10000));
        p %= 10000;
        started = 1;
    }
    if (started || p >= 1000) {
        _fn_tcp_url[offset++] = (char)('0' + (p / 1000));
        p %= 1000;
        started = 1;
    }
    if (started || p >= 100) {
        _fn_tcp_url[offset++] = (char)('0' + (p / 100));
        p %= 100;
        started = 1;
    }
    if (started || p >= 10) {
        _fn_tcp_url[offset++] = (char)('0' + (p / 10));
        p %= 10;
    }
    _fn_tcp_url[offset++] = (char)('0' + p);
    _fn_tcp_url[offset] = '\0';

    return fn_open(handle, 0, _fn_tcp_url, 0);
}
