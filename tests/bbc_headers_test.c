#include <assert.h>
#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_bbc.h"
uint8_t _fn_initialized=1;
char fn_bbc_open_name[128];
static uint8_t old_rom, fail_open, calls, opens;
static const uint8_t expected[]={1,1,0,9,0,'h','t','t','p',':','/','/','a','/',1,0,1,0,'X',1,0,'Y',0,0,0,0,0,0};
static unsigned expected_size = sizeof(expected);
uint8_t fn_bbc_osword78(uint8_t *b) {
    assert(b[0]==9);++calls;
    if(old_rom)return 1;
    if(!b[4]&&!b[5])return 1; /* explicit cleanup */
    assert((unsigned)b[4]+256u*b[5]==expected_size);
    return 0;
}
int fn_bbc_open_flags(uint8_t m) {return m==FN_METHOD_GET ? 0x40 : -1;}
unsigned char osfind(unsigned char mode,const char *name) {
    assert(mode==0x40 && !strcmp(name,"://\r"));++opens;return fail_open ? 0 : 0x15;
}
uint8_t fn_bbc_claim_channel(fn_handle_t *h,unsigned char ch){*h=ch;return 0;}
int main(void) {
    uint8_t guarded[64];fn_handle_t h=0;fn_http_header_t header={"X","Y"};
    fn_bbc_http_open_t r;
    memset(&r,0,sizeof(r));memset(guarded,0xa5,sizeof(guarded));
    r.handle=&h;r.url="http://a/";r.headers=&header;r.count=1;r.method=FN_METHOD_GET;
    r.buffer=guarded+1;r.capacity=sizeof(guarded)-2;
    assert(fn_bbc_open_headers(&r)==0 && h==0x15 && calls==2 && opens==1);
    assert(!memcmp(r.buffer,expected,sizeof(expected)) && guarded[0]==0xa5 && guarded[sizeof(expected)+1]==0xa5);
    calls=opens=0;r.capacity=sizeof(expected)-1;
    assert(fn_bbc_open_headers(&r)==FN_ERR_INVALID && calls==0 && opens==0);
    r.capacity=sizeof(guarded)-2;old_rom=1;
    assert(fn_bbc_open_headers(&r)==FN_ERR_UNSUPPORTED && opens==0);
    old_rom=0;fail_open=1;calls=0;
    assert(fn_bbc_open_headers(&r)==FN_ERR_IO && calls==2);
    fail_open=0;calls=0;r.body_len=1;
    assert(fn_bbc_open_headers(&r)==FN_ERR_INVALID && calls==0);
    r.body_len=0;header.name="";
    assert(fn_bbc_open_headers(&r)==FN_ERR_INVALID && calls==0);
    header.name="X";
    assert(fn_open_headers(&h,FN_METHOD_GET,r.url,0,&header,1,0)==0);
    assert(!memcmp(fn_bbc_open_name,expected,sizeof(expected)));
    {
        uint8_t large[514];
        char url[500];
        memset(large,0xa5,sizeof(large));
        memset(url,'x',sizeof(url)-1); url[sizeof(url)-1]=0;
        r.url=url; r.count=0; r.headers=0;
        r.buffer=large+1; r.capacity=512; expected_size=512;
        assert(fn_bbc_open_headers(&r)==FN_OK);
        assert(large[0]==0xa5 && large[513]==0xa5);
        assert(large[4]==0xf3 && large[5]==1); /* 499-byte URL */
        assert(!memcmp(large+6,url,499));
        assert(large[511]==0 && large[512]==0);
        calls=0; r.capacity=511;
        assert(fn_bbc_open_headers(&r)==FN_ERR_INVALID && calls==0);
        r.capacity=513;
        assert(fn_bbc_open_headers(&r)==FN_ERR_INVALID && calls==0);
    }
    return 0;
}
