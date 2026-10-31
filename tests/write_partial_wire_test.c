#include <stdio.h>
#include <string.h>
#include "fn_internal.h"
#include "fn_platform.h"

static unsigned calls;
static int bad;
static int large;
uint8_t fn_transport_exchange(void)
{
    const uint8_t *req = _fn_transport_ctx.request;
    uint8_t *reply = _fn_transport_ctx.response;
    uint16_t pos;
    ++calls;
    if (large) {
        uint16_t n = FN_READ_LE16(req, 13), i;
        uint32_t offset = FN_READ_LE32(req, 9);
        uint16_t expected = calls == 1 ? FN_MAX_PACKET_SIZE - 15 : 1200 - (FN_MAX_PACKET_SIZE - 15);
        if (n != expected || offset != (calls == 1 ? 0 : FN_MAX_PACKET_SIZE - 15) ||
            _fn_transport_ctx.req_len != 15 + n) bad = 1;
        for (i = 0; i < n; ++i) if (req[15 + i] != (uint8_t)(offset + i)) bad = 1;
        pos = fn_build_header(reply, FN_DEVICE_NETWORK, FN_CMD_WRITE, FN_HEADER_SIZE + 12);
        memset(reply + pos, 0, 12); reply[pos] = 1;
        reply[pos + 10] = (uint8_t)n; reply[pos + 11] = (uint8_t)(n >> 8);
        pos += 12;
        reply[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(reply, pos);
        _fn_transport_ctx.resp_len = pos;
        return FN_OK;
    }
    if (req[0] != FN_DEVICE_NETWORK || req[1] != FN_CMD_WRITE ||
        FN_READ_LE16(req, 7) != 0x1234 ||
        FN_READ_LE32(req, 9) != (calls <= 2 ? 0x0000fffeUL : 0x00010000UL) ||
        FN_READ_LE16(req, 13) != (calls <= 2 ? 4 : 2) ||
        memcmp(req + 15, calls <= 2 ? "ABCD" : "CD", calls <= 2 ? 4 : 2)) bad = 1;
    if (calls == 1) return FN_ERR_NOT_READY;
    pos = fn_build_header(reply, FN_DEVICE_NETWORK, FN_CMD_WRITE, FN_HEADER_SIZE + 12);
    memset(reply + pos, 0, 12);
    reply[pos] = 1;
    reply[pos + 10] = 2; /* peer accepts two bytes at a time */
    pos += 12;
    reply[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(reply, pos);
    _fn_transport_ctx.resp_len = pos;
    return FN_OK;
}
int main(void)
{
    uint16_t written = 99;
    static const uint8_t payload[] = "ABCD";
    _fn_initialized = 1;
    _fn_sessions[0].active = 1;
    _fn_sessions[0].handle = 0x1234;
    _fn_sessions[0].write_offset = 0x0000fffeUL;
    if (fn_write(0x1234, 0xfffeUL, payload, 4, &written) != FN_OK || bad || calls != 3 ||
        written != 4 || _fn_sessions[0].write_offset != 0x00010002UL) return 1;
    if (fn_write(0x1234, 0xfffeUL, payload, 4, &written) != FN_ERR_INVALID || calls != 3) return 2;
    {
        static uint8_t data[1200];
        uint16_t i;
        for (i = 0; i < sizeof(data); ++i) data[i] = (uint8_t)i;
        large = 1; calls = 0; bad = 0; _fn_sessions[0].write_offset = 0;
        if (fn_write(0x1234, 0, data, sizeof(data), &written) != FN_OK || bad || calls != 2 ||
            written != sizeof(data) || _fn_sessions[0].write_offset != sizeof(data)) return 3;
    }
    puts("partial write wire tests passed");
    return 0;
}
