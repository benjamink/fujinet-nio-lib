#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_bbc.h"

static uint16_t put_string(uint8_t *b, uint16_t at, const char *s)
{
    uint16_t n=(uint16_t)strlen(s);
    b[at++]=(uint8_t)n; b[at++]=(uint8_t)(n>>8);
    memcpy(b+at,s,n); return (uint16_t)(at+n);
}

uint8_t __fastcall__ fn_bbc_open_headers(fn_bbc_http_open_t *r)
{
    uint8_t block[16], result, channel, i;
    uint16_t at, n, total, k;
    uint32_t body;
    int mode;
    if (!_fn_initialized || !r || !r->handle || !r->url || !r->buffer ||
        r->capacity<13 || r->capacity>512 || (r->count && !r->headers)) return FN_ERR_INVALID;
    mode=fn_bbc_open_flags(r->method);
    if(mode<0)return FN_ERR_UNSUPPORTED;
    if(r->body_len && r->method!=FN_METHOD_POST && r->method!=FN_METHOD_PUT) return FN_ERR_INVALID;
    n=(uint16_t)strlen(r->url);
    if(!n || n>r->capacity-13)return FN_ERR_INVALID;
    total=(uint16_t)(13+n);
    for(k=0;k<r->count;++k) {
        if(!r->headers[k].name || !*r->headers[k].name || !r->headers[k].value)return FN_ERR_INVALID;
        n=(uint16_t)strlen(r->headers[k].name);
        if(r->capacity-total<4 || n>r->capacity-total-4)return FN_ERR_INVALID;
        total=(uint16_t)(total+4+n);
        n=(uint16_t)strlen(r->headers[k].value);
        if(n>r->capacity-total)return FN_ERR_INVALID;
        total=(uint16_t)(total+n);
    }
    r->buffer[0]=1; r->buffer[1]=r->method;
    r->buffer[2]=r->flags & 0x3e; /* TLS is selected by URL, as in the shared API. */
    at=put_string(r->buffer,3,r->url);
    r->buffer[at++]=(uint8_t)r->count; r->buffer[at++]=(uint8_t)(r->count>>8);
    for(k=0;k<r->count;++k) {
        at=put_string(r->buffer,at,r->headers[k].name);
        at=put_string(r->buffer,at,r->headers[k].value);
    }
    body=r->body_len;
    for(i=0;i<4;++i){r->buffer[at++]=(uint8_t)body;body>>=8;}
    r->buffer[at++]=0;r->buffer[at++]=0;
    memset(block,0,sizeof(block));block[0]=9;
    block[2]=(uint8_t)(unsigned long)r->buffer;
    block[3]=(uint8_t)((unsigned long)r->buffer>>8);
    block[4]=(uint8_t)at;block[5]=(uint8_t)(at>>8);
    result=fn_bbc_osword78(block);
    if(result)return FN_ERR_UNSUPPORTED;
    channel=osfind((uint8_t)mode,"://\r");
    /* Cancel even when OSFIND failed before consuming the one-shot pointer. */
    block[0]=9;block[4]=block[5]=0;
    (void)fn_bbc_osword78(block);
    if(!channel)return FN_ERR_IO;
    return fn_bbc_claim_channel(r->handle,channel);
}
