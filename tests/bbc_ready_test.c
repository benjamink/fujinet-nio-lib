#include <assert.h>
#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_raw.h"
fn_session_t _fn_sessions[FN_MAX_SESSIONS];
uint8_t _fn_initialized;
static uint8_t old_rom, malformed, device_status, calls;
uint8_t fn_bbc_osword78(uint8_t *b) {
    assert(b[0]==8);
    if(old_rom)return 1;
    b[3]=b[2]==0x15 ? 0x34 : 0x78; b[4]=0x12;
    b[5]=b[2]==0x15 ? 3 : 0; b[6]=0; b[7]=b[5]!=0;
    return 0;
}
uint8_t fn_raw_call(uint8_t dev,uint8_t cmd,const void *p,uint16_t n,
    void *out,uint16_t cap,fn_raw_response_t *r) {
    static const uint8_t wire[]={1,0,0,0,2,0x34,0x12,1,0xfe,0xff,0x78,0x12,2,0,0};
    assert(dev==0xfd && cmd==8 && n==1 && *(const uint8_t*)p==1 && cap>=sizeof(wire));
    ++calls;memcpy(out,wire,sizeof(wire));r->status=device_status;
    r->payload_length=malformed ? 4 : sizeof(wire);return 0;
}
int main(void) {
    fn_ready_entry_t e[2];uint8_t n=99;
    assert(fn_ready(e,2,&n)==FN_ERR_INVALID);
    _fn_initialized=1;
    _fn_sessions[0].active=1;_fn_sessions[0].handle=0x15;
    _fn_sessions[1].active=1;_fn_sessions[1].handle=0x14;
    assert(fn_ready(e,2,&n)==0 && n==2 && calls==1);
    assert(e[0].handle==0x15 && e[0].flags==1 && e[0].available==65535u);
    assert(e[1].handle==0x14 && e[1].flags==2 && e[1].available==0);
    assert(fn_ready(e,1,&n)==0 && n==1);
    old_rom=1;calls=0;assert(fn_ready(e,2,&n)==FN_ERR_UNSUPPORTED && calls==0 && n==0);
    old_rom=0;malformed=1;assert(fn_ready(e,2,&n)==FN_ERR_IO && n==0);
    malformed=0;device_status=8;assert(fn_ready(e,2,&n)==FN_ERR_UNSUPPORTED);
    assert(fn_ready(0,1,&n)==FN_ERR_INVALID);
    return 0;
}
