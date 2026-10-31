#include "fn_internal.h"
#include "fn_platform.h"

#include <string.h>

#ifndef FN_WRITE_RETRY_LIMIT
#define FN_WRITE_RETRY_LIMIT 500
#endif

static fn_write_diagnostics_t last_write_diagnostics;

void fn_write_get_last_diagnostics(fn_write_diagnostics_t *diagnostics)
{
    if (diagnostics != NULL) {
        *diagnostics = last_write_diagnostics;
    }
}

uint8_t fn_write(fn_handle_t handle,
                 uint32_t offset,
                 const uint8_t *data,
                 uint16_t len,
                 uint16_t *written)
{
    uint16_t req_len;
    uint16_t total;
    uint16_t accepted;
    uint16_t remaining;
    uint16_t retries;
    uint8_t result;
    int8_t slot;

    memset(&last_write_diagnostics, 0, sizeof(last_write_diagnostics));

    if (written != NULL) {
        *written = 0;
    }

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == FN_INVALID_HANDLE) {
        return FN_ERR_INVALID;
    }

    if (len > 0 && data == NULL) {
        return FN_ERR_INVALID;
    }

    slot = fn_find_session(handle);
    if (slot < 0) {
        return FN_ERR_NOT_FOUND;
    }

    if (offset != _fn_sessions[slot].write_offset) {
        return FN_ERR_INVALID;
    }

    total = 0;
    retries = FN_WRITE_RETRY_LIMIT;

    do {
        remaining = (uint16_t)(len - total);

        req_len = fn_build_write_packet(_fn_req_buf,
                                        handle,
                                        offset + total,
                                        (data == NULL) ? NULL : data + total,
                                        remaining);
        if (req_len == 0) {
            return FN_ERR_INVALID;
        }

        _fn_transport_ctx.request = _fn_req_buf;
        _fn_transport_ctx.req_len = req_len;
        _fn_transport_ctx.response = _fn_resp_buf;
        _fn_transport_ctx.resp_max = FN_MAX_PACKET_SIZE;

        last_write_diagnostics.exchanges++;
        result = fn_transport_exchange();
        if (result == FN_ERR_NOT_READY || result == FN_ERR_BUSY) {
            last_write_diagnostics.transport_retry++;
            if (retries-- > 0) {
                continue;
            }
            return result;
        }
        if (result != FN_OK) {
            return result;
        }

        last_write_diagnostics.response_length = _fn_transport_ctx.resp_len;
        if (_fn_transport_ctx.resp_len >= 2) {
            last_write_diagnostics.response_device = _fn_resp_buf[0];
            last_write_diagnostics.response_command = _fn_resp_buf[1];
        }

        _fn_parse_ctx.response = _fn_resp_buf;
        _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
        result = fn_parse_response_header();
        if (result != FN_OK) {
            return result;
        }

        if (_fn_parse_ctx.status == FN_ERR_NOT_READY ||
            _fn_parse_ctx.status == FN_ERR_BUSY) {
            last_write_diagnostics.service_retry++;
            if (retries-- > 0) {
                continue;
            }
            return _fn_parse_ctx.status;
        }

        if (_fn_parse_ctx.status != FN_OK) {
            return _fn_parse_ctx.status;
        }

        if (_fn_parse_ctx.data_len >= 12) {
            accepted = FN_READ_LE16(_fn_resp_buf, _fn_parse_ctx.data_offset + 10);
        } else {
            accepted = 0;
        }

        if (accepted > remaining) {
            return FN_ERR_IO;
        }

        if (len == 0) {
            break;
        }

        if (accepted == 0) {
            last_write_diagnostics.zero_accepted++;
            if (retries-- > 0) {
                continue;
            }
            return FN_ERR_BUSY;
        }

        _fn_sessions[slot].write_offset += accepted;
        total = (uint16_t)(total + accepted);
        retries = FN_WRITE_RETRY_LIMIT;
    } while (total < len);

    if (written != NULL) {
        *written = total;
    }

    return FN_OK;
}
