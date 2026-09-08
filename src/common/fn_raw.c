#include <string.h>

#include "fn_internal.h"
#include "fn_platform.h"
#include "fn_raw.h"

uint8_t fn_raw_call(uint8_t device,
                    uint8_t command,
                    const void *payload,
                    uint16_t payload_length,
                    void *reply,
                    uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    uint16_t req_len;
    uint16_t copy_len;
    uint8_t result;
    uint8_t retried;
    uint8_t attempt_ok;

    if (response != 0) {
        response->status = FN_ERR_INTERNAL;
        response->payload_length = 0;
    }

    if (!_fn_initialized) {
        result = fn_init();
        if (result != FN_OK) {
            return result;
        }
    }

    if (payload_length > 0 && payload == 0) {
        return FN_ERR_INVALID;
    }
    if (payload_length > (uint16_t)(FN_MAX_PACKET_SIZE - FN_HEADER_SIZE)) {
        return FN_ERR_INVALID;
    }

    req_len = fn_build_header(_fn_req_buf,
                              device,
                              command,
                              (uint16_t)(FN_HEADER_SIZE + payload_length));
    if (payload_length > 0) {
        memcpy(_fn_req_buf + req_len, payload, payload_length);
        req_len = (uint16_t)(req_len + payload_length);
    }
    _fn_req_buf[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(_fn_req_buf, req_len);

    _fn_transport_ctx.request = _fn_req_buf;
    _fn_transport_ctx.req_len = req_len;
    _fn_transport_ctx.response = _fn_resp_buf;
    _fn_transport_ctx.resp_max = FN_MAX_PACKET_SIZE;

    retried = 0;
    copy_len = 0;

    for (;;) {
        attempt_ok = 0;

        _fn_transport_ctx.resp_len = 0;
        result = fn_transport_exchange();
        if (result == FN_OK &&
            _fn_transport_ctx.resp_len >= FN_HEADER_SIZE &&
            _fn_resp_buf[0] == device &&
            _fn_resp_buf[1] == command) {
            _fn_parse_ctx.response = _fn_resp_buf;
            _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
            result = fn_parse_response_header();
            if (result == FN_OK) {
                copy_len = _fn_parse_ctx.data_len;
                if (copy_len > reply_capacity)
                    result = FN_ERR_IO;
                else
                    attempt_ok = 1;
            }
        } else if (result == FN_OK) {
            result = FN_ERR_IO;
        }

        if (attempt_ok)
            break;
        /* One replay after a short/corrupt body. Do not retry TIMEOUT
         * (multi-second QUERY). Disk I/O does not use fn_raw_call. */
        if (retried || result == FN_ERR_TIMEOUT)
            return result;
        retried = 1;
    }
    if (copy_len > 0 && reply != 0) {
        memcpy(reply, _fn_resp_buf + _fn_parse_ctx.data_offset, copy_len);
    }

    if (response != 0) {
        response->status = _fn_parse_ctx.status;
        response->payload_length = copy_len;
    }

    return FN_OK;
}
