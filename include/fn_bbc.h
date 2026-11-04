#ifndef FN_BBC_H
#define FN_BBC_H
#include "fujinet-nio.h"
/* BBC-only, synchronous. Caller storage and pointed-to strings must remain
 * valid until return. Buffer must not overlap any input or output objects.
 * Capacity is 13..512 bytes. Requires ROM OSWORD &78 reason &09. */
typedef struct {
    fn_handle_t *handle;
    const char *url;
    const fn_http_header_t *headers;
    uint8_t *buffer;
    uint16_t capacity;
    uint16_t count;
    uint32_t body_len;
    uint8_t method;
    uint8_t flags;
} fn_bbc_http_open_t;
#ifdef __cplusplus
extern "C" {
#endif
uint8_t __fastcall__ fn_bbc_open_headers(fn_bbc_http_open_t *request);
#ifdef __cplusplus
}
#endif
#endif
