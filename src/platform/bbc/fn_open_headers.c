#include "fn_bbc_internal.h"
#include "fn_bbc.h"

/* Reuse the existing short-open workspace. No extra persistent header buffer.
 * Use fn_bbc_open_headers with caller storage for requests exceeding 128 bytes. */
uint8_t fn_open_headers(fn_handle_t *handle, uint8_t method, const char *url,
                        uint8_t flags, const fn_http_header_t *headers,
                        uint16_t count, uint32_t body_len)
{
    fn_bbc_http_open_t r;
    r.handle=handle; r.method=method; r.url=url; r.flags=flags;
    r.headers=headers; r.count=count; r.body_len=body_len;
    r.buffer=(uint8_t*)fn_bbc_open_name; r.capacity=128;
    return fn_bbc_open_headers(&r);
}
