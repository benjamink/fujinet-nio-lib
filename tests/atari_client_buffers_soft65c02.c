#include <stdint.h>
#include <string.h>
#include "fn_atari_buffers.h"
#include "fn_internal.h"
#include "fn_raw.h"

/* These definitions are application-owned; the archive must not allocate its
 * own packet buffers. The non-overlap is intentional and required. */
FN_ATARI_DEFINE_PACKET_BUFFERS();
uint8_t test_result, test_seen;
static uint8_t calls;
uint8_t fn_transport_init(void) { return FN_OK; }
uint8_t fn_transport_ready(void) { return 1; }
uint8_t fn_transport_exchange(void)
{
    static const uint8_t expected[] = {0xfe, 5, 8, 0, 0x3f, 0, 0x11, 0x22};
    static const uint8_t answer[] = {0xfe, 5, 7, 0, 0xb6, 0, 0xab};
    ++calls;
    if (_fn_transport_ctx.request != _fn_req_buf || _fn_transport_ctx.response != _fn_resp_buf ||
        _fn_transport_ctx.req_len != sizeof(expected) || memcmp(_fn_req_buf, expected, sizeof(expected)))
        return FN_ERR_IO;
    memcpy(_fn_resp_buf, answer, sizeof(answer));
    _fn_transport_ctx.resp_len = sizeof(answer);
    return FN_OK;
}
void test_func(void)
{
    static const uint8_t payload[] = {0x11, 0x22};
    uint8_t out = 0;
    fn_raw_response_t reply;
    test_seen = 0x5a; test_result = 0xff;
    _fn_req_buf[511] = 0x77; _fn_resp_buf[511] = 0x88;
    if (fn_raw_call(0xfe, 5, payload, sizeof(payload), &out, 1, &reply) != FN_OK ||
        reply.status != 0 || reply.payload_length != 1 || out != 0xab || calls != 1 ||
        _fn_req_buf[511] != 0x77 || _fn_resp_buf[511] != 0x88) { test_result = 1; return; }
    test_result = 0;
}
