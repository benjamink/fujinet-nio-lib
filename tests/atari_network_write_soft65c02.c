#include <stdint.h>
#include <string.h>
#include "fn_internal.h"
#include "fn_platform.h"
uint8_t test_case, test_seen, test_result;
fn_session_t _fn_sessions[FN_MAX_SESSIONS];
uint8_t _fn_initialized, _fn_req_buf[FN_MAX_PACKET_SIZE], _fn_resp_buf[FN_MAX_PACKET_SIZE];
fn_transport_ctx_t _fn_transport_ctx;
fn_parse_ctx_t _fn_parse_ctx;
static uint8_t data[600], bad;
static uint16_t builder_calls, transport_calls, parse_calls;
static uint16_t expected_handle, size;
static uint32_t expected_initial;
uint8_t profile_network_write(fn_handle_t, uint32_t, const uint8_t *, uint16_t, uint16_t *);
int8_t fn_find_session(fn_handle_t h) {
    if (h == 0x1234 && _fn_sessions[0].active) return 0;
    if (h == 0x5678 && _fn_sessions[1].active) return 1;
    return -1;
}
uint16_t fn_build_write_packet(uint8_t *buffer, fn_handle_t h, uint32_t off, const uint8_t *p, uint16_t n) {
    uint16_t progressed = ((test_case == 7 || test_case == 21) && builder_calls > 0) ? 2 :
                          ((test_case == 16 && builder_calls > 0) ? 497 : 0);
    uint16_t want = size - progressed;
    if (want > 497) want = 497;
    ++builder_calls;
    if (buffer != _fn_req_buf || h != expected_handle || off != expected_initial + progressed ||
        n != want || p != (test_case == 14 ? 0 : data + progressed)) bad=1;
    if (test_case==20) return 0;
    return (uint16_t)(15 + n);
}
uint8_t fn_transport_exchange(void) {
    ++transport_calls;
    if (_fn_transport_ctx.request != _fn_req_buf || _fn_transport_ctx.response != _fn_resp_buf ||
        _fn_transport_ctx.resp_max != 512) bad=2;
    if (test_case==12 || (test_case==21 && transport_calls==2)) return FN_ERR_IO;
    if ((test_case==8 && transport_calls==1) || test_case==17) return FN_ERR_NOT_READY;
    _fn_resp_buf[0]=FN_DEVICE_NETWORK; _fn_resp_buf[1]=FN_CMD_WRITE;
    _fn_transport_ctx.resp_len=20;
    return 0;
}
uint8_t fn_parse_response_header(void) {
    uint16_t n;
    ++parse_calls;
    if (_fn_parse_ctx.response!=_fn_resp_buf || _fn_parse_ctx.resp_len!=20) bad=3;
    if (test_case==13) return FN_ERR_INVALID;
    _fn_parse_ctx.status=test_case==22 ? FN_ERR_IO :
                         (((test_case==9 && parse_calls==1) || test_case==19) ? FN_ERR_BUSY : 0);
    _fn_parse_ctx.data_offset=7; _fn_parse_ctx.data_len=test_case==18?11:12;
    n = test_case==14 ? 0 : (test_case==11 ? 5 : (test_case==7 || test_case==21 ? 2 :
        (test_case==16 ? (parse_calls==1 ? 497 : 103) : 4)));
    if (test_case==10 && parse_calls==1) n=0;
    _fn_resp_buf[17]=(uint8_t)n; _fn_resp_buf[18]=(uint8_t)(n>>8);
    return 0;
}
void test_func(void) {
    uint16_t wrote=0xa5a5, *out=&wrote;
    uint8_t *ptr=data, expected=0, result, slot=0;
    fn_write_diagnostics_t d;
    test_seen=test_case; test_result=0xff; builder_calls=transport_calls=parse_calls=bad=0;
    memset(_fn_sessions,0,sizeof(_fn_sessions));
    memset(&_fn_transport_ctx,0,sizeof(_fn_transport_ctx));
    memset(&_fn_parse_ctx,0,sizeof(_fn_parse_ctx));
    _fn_initialized=1; size=4; expected_handle=0x1234; expected_initial=0x0000fffeUL;
    _fn_sessions[0].active=1; _fn_sessions[0].handle=expected_handle;
    _fn_sessions[0].write_offset=expected_initial;
    if (test_case==1) { _fn_initialized=0; expected=FN_ERR_INVALID; }
    if (test_case==2) { expected_handle=0; expected=FN_ERR_INVALID; }
    if (test_case==3) { ptr=0; expected=FN_ERR_INVALID; }
    if (test_case==4) { expected_handle=0x9999; expected=FN_ERR_NOT_FOUND; }
    if (test_case==5) { expected_initial=7; expected=FN_ERR_INVALID; }
    if (test_case==6) { slot=1; expected_handle=0x5678; _fn_sessions[1]=_fn_sessions[0]; _fn_sessions[1].handle=expected_handle; }
    if (test_case==11 || test_case==12) expected=FN_ERR_IO;
    if (test_case==13) expected=FN_ERR_INVALID;
    if (test_case==14) {size=0;ptr=0;}
    if (test_case==15) out=0;
    if (test_case==16) size=600;
    if (test_case==17) expected=FN_ERR_NOT_READY;
    if (test_case==19) expected=FN_ERR_BUSY;
    if (test_case==18) expected=FN_ERR_BUSY;
    if (test_case==20) expected=FN_ERR_INVALID;
    if (test_case==21) expected=FN_ERR_IO;
    if (test_case==22) expected=FN_ERR_IO;
    result=profile_network_write(expected_handle,expected_initial,ptr,size,out);
    fn_write_get_last_diagnostics(0); /* optional destination must be safe */
    memset(&d,0xa5,sizeof(d)); fn_write_get_last_diagnostics(&d);
    if (result!=expected || bad || (out && wrote!=(expected?0:size))) {test_result=1;return;}
    if (test_case>=1 && test_case<=5) {
        if (builder_calls || transport_calls || parse_calls || d.exchanges) {test_result=2;return;}
    } else if (test_case==20) {
        if (builder_calls!=1 || transport_calls || parse_calls || d.exchanges) {test_result=6;return;}
    } else {
        uint16_t calls=(test_case==17||test_case==18||test_case==19)?501:((test_case==7||test_case==8||test_case==9||test_case==10||test_case==16||test_case==21)?2:1);
        if (builder_calls!=calls || transport_calls!=calls || d.exchanges!=calls ||
            d.transport_retry!=(test_case==17?501:(test_case==8?1:0)) ||
            d.service_retry!=(test_case==19?501:(test_case==9?1:0)) ||
            d.zero_accepted!=(test_case==18?501:(test_case==10?1:0))) {test_result=3;return;}
        if (test_case!=12 && test_case!=17 && (d.response_length!=20 || d.response_device!=FN_DEVICE_NETWORK || d.response_command!=FN_CMD_WRITE)) {test_result=4;return;}
        if (_fn_sessions[slot].write_offset != expected_initial + (test_case==21?2:(expected?0:size))) {test_result=5;return;}
    }
    test_result=0;
}
