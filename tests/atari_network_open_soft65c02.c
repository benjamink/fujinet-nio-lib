#include <stdint.h>
#include <string.h>
#include "fn_internal.h"
#include "fn_platform.h"
uint8_t test_case, test_seen, test_result;
fn_session_t _fn_sessions[FN_MAX_SESSIONS];
uint8_t _fn_initialized, _fn_req_buf[FN_MAX_PACKET_SIZE], _fn_resp_buf[FN_MAX_PACKET_SIZE];
fn_transport_ctx_t _fn_transport_ctx;
fn_parse_ctx_t _fn_parse_ctx;
static uint8_t trans_calls, parse_calls, bad;
uint8_t profile_open_exchange(fn_handle_t *, uint16_t);
int8_t fn_find_free_slot(void) {
    if (!_fn_sessions[0].active) return 0;
    if (!_fn_sessions[1].active) return 1;
    return -1;
}
uint8_t fn_transport_exchange(void) {
    ++trans_calls;
    if (_fn_transport_ctx.request!=_fn_req_buf || _fn_transport_ctx.response!=_fn_resp_buf ||
        _fn_transport_ctx.req_len!=0x1234 || _fn_transport_ctx.resp_max!=512) bad=1;
    if (test_case==3) return FN_ERR_TIMEOUT;
    _fn_transport_ctx.resp_len=42;
    return 0;
}
uint8_t fn_parse_open_response(void) {
    ++parse_calls;
    if (_fn_parse_ctx.response!=_fn_resp_buf || _fn_parse_ctx.resp_len!=42) bad=2;
    if (test_case==4) return FN_ERR_IO;
    _fn_parse_ctx.handle=0xbeef; _fn_parse_ctx.proto_flags=7;
    _fn_parse_ctx.flags=test_case==5?0:(test_case==6?0x80:FN_OPEN_RESP_NEEDS_BODY);
    return 0;
}
void test_func(void) {
    fn_handle_t handle=0xa5a5;
    uint8_t slot=0, result, expected=0;
    test_seen=test_case; test_result=0xff; trans_calls=parse_calls=bad=0;
    memset(_fn_sessions,0xa5,sizeof(_fn_sessions));
    memset(&_fn_transport_ctx,0xa5,sizeof(_fn_transport_ctx));
    memset(&_fn_parse_ctx,0xa5,sizeof(_fn_parse_ctx));
    _fn_sessions[0].active=0; _fn_sessions[1].active=0;
    if (test_case==1) { _fn_sessions[0].active=1; slot=1; }
    if (test_case==2) { _fn_sessions[0].active=1; _fn_sessions[1].active=1; expected=FN_ERR_NO_HANDLES; }
    if (test_case==3) expected=FN_ERR_TIMEOUT;
    if (test_case==4) expected=FN_ERR_IO;
    result=profile_open_exchange(&handle,0x1234);
    if (result!=expected || bad) {test_result=1;return;}
    if (expected) {
        if (handle || trans_calls!=(test_case==2?0:1) || parse_calls!=(test_case==4?1:0)) {test_result=2;return;}
        if (test_case!=2 && _fn_sessions[slot].active) {test_result=3;return;}
        if (test_case==2 && _fn_transport_ctx.req_len!=0xa5a5) {test_result=4;return;}
    } else if (handle!=0xbeef || trans_calls!=1 || parse_calls!=1 ||
               _fn_sessions[slot].active!=1 || _fn_sessions[slot].proto_flags!=7 ||
               _fn_sessions[slot].needs_body!=(test_case==5||test_case==6?0:1) ||
               _fn_sessions[slot].handle!=0xbeef || _fn_sessions[slot].reserved!=0xa5 ||
               _fn_sessions[slot].read_offset || _fn_sessions[slot].write_offset) {test_result=5;return;}
    test_result=0;
}
