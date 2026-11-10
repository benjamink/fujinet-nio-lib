/* Palm-only exposure of existing NetworkService TranslateConfigure (0x07).
 * Reuse the shared packet header/checksum/parser and existing request buffers.
 * No HTTP, JSON parser, new wire command or transport implementation here. */
#include <string.h>
#include "private.h"
#include "fn_internal.h"
#include "fn_platform.h"

UInt16 nl_json_query(UInt16 handle, const Char *selector)
{
    UInt16 length = 0, offset, result;
    Int16 slot = fn_find_session(handle);
    if (slot < 0) return FN_ERR_NOT_FOUND;
    /* Six-byte header + version/handle + type/flags/selector length = 13. */
    while (selector[length]) {
        if (length == 1024 - 13) return FN_ERR_INVALID;
        ++length;
    }
    offset = fn_build_header(_fn_req_buf, FN_DEVICE_NETWORK, 0x07, 13 + length);
    _fn_req_buf[offset++] = FN_PROTOCOL_VERSION;
    _fn_req_buf[offset++] = (UInt8)handle;
    _fn_req_buf[offset++] = (UInt8)(handle >> 8);
    _fn_req_buf[offset++] = FN_TRANSLATE_JSON;
    _fn_req_buf[offset++] = 0;
    _fn_req_buf[offset++] = (UInt8)length;
    _fn_req_buf[offset++] = (UInt8)(length >> 8);
    if (length) memcpy(_fn_req_buf + offset, selector, length);
    offset += length;
    _fn_req_buf[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(_fn_req_buf, offset);
    _fn_transport_ctx.request = _fn_req_buf; _fn_transport_ctx.req_len = offset;
    _fn_transport_ctx.response = _fn_resp_buf; _fn_transport_ctx.resp_max = 1024;
    _fn_transport_ctx.resp_len = 0;
    result = fn_transport_exchange();
    if (result) return result;
    _fn_parse_ctx.response = _fn_resp_buf;
    _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
    result = fn_parse_response_header();
    if (!result) result = _fn_parse_ctx.status;
    /* A new selector starts a new translated value at offset zero. Keep the
     * common sequential-read guard, resetting only this owned handle's cursor. */
    if (result == FN_OK || result == FN_ERR_NOT_READY) _fn_sessions[slot].read_offset = 0;
    return result;
}
