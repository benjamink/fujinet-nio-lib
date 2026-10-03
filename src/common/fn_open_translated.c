#include "fn_internal.h"
#include "fn_platform.h"

/* Own file so programs that only call fn_open() don't link the translation
   packet builder. */
uint8_t fn_open_translated(fn_handle_t *handle,
                           uint8_t method,
                           const char *url,
                           uint8_t flags,
                           uint8_t ttype,
                           uint8_t tflags,
                           const char *selector)
{
    uint16_t req_len;
    uint8_t open_flags;
    uint8_t result;

    result = fn_open_prepare(handle, url, flags, &open_flags);
    if (result != FN_OK) {
        return result;
    }

    req_len = fn_build_open_packet_ext(_fn_req_buf, method, open_flags, url,
                                       ttype, tflags, selector);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    return fn_open_exchange(handle, req_len);
}
