#include <stdint.h>
#include <string.h>
#include "fn_internal.h"
#include "fn_platform.h"
uint8_t test_case, test_seen, test_result;
fn_session_t _fn_sessions[FN_MAX_SESSIONS];
uint8_t _fn_initialized, _fn_req_buf[FN_MAX_PACKET_SIZE], _fn_resp_buf[FN_MAX_PACKET_SIZE];
fn_transport_ctx_t _fn_transport_ctx;
fn_parse_ctx_t _fn_parse_ctx;
static uint8_t output[512], builder_calls, transport_calls, parse_calls, bad;
static uint16_t expected_handle, expected_limit;
static uint32_t expected_offset;
uint8_t profile_network_read(fn_handle_t, uint32_t, uint8_t *, uint16_t, uint16_t *, uint8_t *);
int8_t fn_find_session(fn_handle_t h) {
    if (h == 0x1234 && _fn_sessions[0].active) return 0;
    if (h == 0x5678 && _fn_sessions[1].active) return 1;
    return -1;
}
uint16_t fn_build_read_packet(uint8_t *buffer, fn_handle_t h, uint32_t off, uint16_t n) {
    ++builder_calls;
    if (buffer != _fn_req_buf || h != expected_handle || off != expected_offset || n != expected_limit) bad=1;
    if (test_case==16) return 0;
    return 15;
}
uint8_t fn_transport_exchange(void) {
    ++transport_calls;
    if (_fn_transport_ctx.request != _fn_req_buf || _fn_transport_ctx.response != _fn_resp_buf ||
        _fn_transport_ctx.req_len != 15 || _fn_transport_ctx.resp_max != 512) bad=2;
    if (test_case == 9) return FN_ERR_IO;
    _fn_transport_ctx.resp_len = 25;
    return FN_OK;
}
uint8_t fn_parse_read_response(void) {
    ++parse_calls;
    if (_fn_parse_ctx.response != _fn_resp_buf || _fn_parse_ctx.resp_len != 25 ||
        _fn_parse_ctx.data != output || _fn_parse_ctx.data_max != expected_limit) bad=3;
    if (test_case == 10) return FN_ERR_INVALID;
    _fn_parse_ctx.data_len = test_case == 13 ? 0 : 4;
    _fn_parse_ctx.flags = FN_READ_AVAILABLE;
    _fn_parse_ctx.available = 0x1234;
    if (_fn_parse_ctx.data_len) memcpy(output, "TEST", 4);
    return FN_OK;
}
void test_func(void) {
    uint16_t got=0xa5a5, size=16;
    uint8_t flags=0xa5, result, expected=FN_OK, slot=0;
    uint8_t *buf=output, *flagp=&flags;
    uint16_t *outp=&got;
    test_seen=test_case; test_result=0xff; builder_calls=transport_calls=parse_calls=bad=0;
    memset(_fn_sessions,0,sizeof(_fn_sessions));
    memset(output,0xa5,sizeof(output));
    _fn_initialized=1;
    expected_handle=0x1234; expected_offset=0x00fffffeUL;
    _fn_sessions[0].active=1; _fn_sessions[0].handle=expected_handle;
    _fn_sessions[0].proto_flags=FN_PROTO_FLAG_SEQUENTIAL_READ;
    _fn_sessions[0].read_offset=expected_offset;
    if (test_case==1) { _fn_initialized=0; expected=FN_ERR_INVALID; }
    if (test_case==2) { expected_handle=0; expected=FN_ERR_INVALID; }
    if (test_case==3) { buf=0; expected=FN_ERR_INVALID; }
    if (test_case==4) { outp=0; expected=FN_ERR_INVALID; }
    if (test_case==5) { expected_handle=0x9999; expected=FN_ERR_NOT_FOUND; }
    if (test_case==6) { expected_offset=7; expected=FN_ERR_INVALID; }
    if (test_case==7) { _fn_sessions[0].proto_flags=0; expected_offset=7; }
    if (test_case==8) size=500;
    if (test_case==9) expected=FN_ERR_IO;
    if (test_case==10) expected=FN_ERR_INVALID;
    if (test_case==11) flagp=0;
    if (test_case==12) { slot=1; expected_handle=0x5678; _fn_sessions[1]=_fn_sessions[0]; _fn_sessions[1].handle=expected_handle; }
    if (test_case==13) size=0;
    if (test_case==14) size=490;
    if (test_case==15) size=491;
    if (test_case==16) expected=FN_ERR_INVALID;
    if (test_case==17) flagp=(uint8_t *)&got;
    expected_limit = size > 490 ? 490 : size;
    result=profile_network_read(expected_handle, expected_offset, buf, size, outp, flagp);
    if (result!=expected || bad) {test_result=1; return;}
    if (test_case<=6 && test_case!=0) {
        if (builder_calls || transport_calls || parse_calls) {test_result=2;return;}
        if (test_case>=5 && (got!=0 || flags!=0)) {test_result=3;return;}
    } else if (test_case==16) {
        if (builder_calls!=1 || transport_calls || parse_calls || got || flags) {test_result=10;return;}
    } else {
        if (builder_calls!=1 || transport_calls!=1 || parse_calls!=(test_case==9?0:1)) {test_result=4;return;}
        if (test_case==9 || test_case==10) { if (got || flags) {test_result=5;return;} }
        else {
            if (got!=(test_case==13?0:4) || (flagp && test_case!=17 && flags!=FN_READ_AVAILABLE) ||
                fn_read_available()!=0x1234) {test_result=6;return;}
            if (test_case!=13 && memcmp(output,"TEST",4)) {test_result=7;return;}
            if (test_case==7) { if (_fn_sessions[slot].read_offset!=0x00fffffeUL) {test_result=8;return;} }
            else if (_fn_sessions[slot].read_offset!=(test_case==13?0x00fffffeUL:0x01000002UL)) {test_result=9;return;}
        }
    }
    test_result=0;
}
