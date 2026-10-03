#include <string.h>

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

    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == NULL || url == NULL) {
        return FN_ERR_INVALID;
    }

    if (strlen(url) > FN_MAX_URL_LEN) {
        return FN_ERR_URL_TOO_LONG;
    }

    open_flags = 0;
    if (flags & FN_OPEN_FOLLOW_REDIR) {
        open_flags |= FN_OPEN_FLAG_FOLLOW_REDIR;
    }
    if (flags & FN_OPEN_BODY_UNKNOWN) {
        open_flags |= FN_OPEN_FLAG_BODY_UNKNOWN;
    }
    if (flags & FN_OPEN_ALLOW_EVICT) {
        open_flags |= FN_OPEN_FLAG_ALLOW_EVICT;
    }
    if (flags & FN_OPEN_STREAM_NO_PROBE) {
        open_flags |= FN_OPEN_FLAG_STREAM_NO_PROBE;
    }

    req_len = fn_build_open_packet_ext(_fn_req_buf, method, open_flags, url,
                                       ttype, tflags, selector);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    return fn_open_exchange(handle, req_len);
}
