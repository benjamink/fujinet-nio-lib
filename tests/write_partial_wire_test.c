#include <stdio.h>
#include <string.h>
#include "fn_internal.h"
#include "fn_platform.h"

static unsigned calls;
static int bad;
uint8_t fn_transport_exchange(void)
{
    const uint8_t *req = _fn_transport_ctx.request;
    uint8_t *reply = _fn_transport_ctx.response;
    uint16_t pos;
    ++calls;
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
    puts("partial write wire tests passed");
    return 0;
}
