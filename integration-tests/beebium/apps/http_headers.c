#include <conio.h>
#include "fujinet-nio.h"
#include "fn_bbc.h"
static uint8_t wire[512];
static const fn_http_header_t headers[]={{"Authorization","Bearer bbc-test"},{"X-Client","BBC"}};
int main(void) {
    fn_handle_t h; fn_bbc_http_open_t r; uint8_t e;
    if(fn_init())return 1;
    e=fn_open_headers(&h,FN_METHOD_GET,"http://example.com/",0,headers,2,0);
    if(e){cputs("HEADERS FAIL");return 1;}
    fn_close(h);
    r.handle=&h;r.url="http://example.com/upload";r.headers=headers;r.count=2;
    r.buffer=wire;r.capacity=sizeof(wire);r.body_len=0x12345678UL;r.method=FN_METHOD_POST;r.flags=FN_OPEN_KEEP;
    if(fn_set_content_profile(1))return 1;
    if(fn_bbc_open_headers(&r)){cputs("BUFFER FAIL");return 1;}
    fn_close(h);
    /* Both opens must consume their one-shot state. */
    if(fn_open(&h,FN_METHOD_GET,"http://example.com/plain",0)){cputs("PLAIN FAIL");return 1;}
    fn_close(h);cputs("HEADERS OK");return 0;
}
