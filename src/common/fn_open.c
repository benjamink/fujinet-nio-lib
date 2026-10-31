#include <string.h>

#include "fn_internal.h"
#include "fn_platform.h"

/* Shared by fn_open() and fn_open_translated(): check the arguments and map the
   public FN_OPEN_* flags to the wire flags. Keep every open-flag mapping here so
   the two calls cannot drift apart. */
uint8_t fn_open_prepare(const fn_handle_t *handle,
                        const char *url,
                        uint8_t flags,
                        uint8_t *wire_flags)
{
    if (!_fn_initialized) {
        return FN_ERR_INVALID;
    }

    if (handle == NULL || url == NULL) {
        return FN_ERR_INVALID;
    }

    if (strlen(url) > FN_MAX_URL_LEN) {
        return FN_ERR_URL_TOO_LONG;
    }

    /* These public bits deliberately mirror the protocol bits. Assert that
       contract so a future flag change cannot silently alter the wire. */
#if FN_OPEN_FOLLOW_REDIR != FN_OPEN_FLAG_FOLLOW_REDIR || \
    FN_OPEN_BODY_UNKNOWN != FN_OPEN_FLAG_BODY_UNKNOWN || \
    FN_OPEN_ALLOW_EVICT != FN_OPEN_FLAG_ALLOW_EVICT || \
    FN_OPEN_STREAM_NO_PROBE != FN_OPEN_FLAG_STREAM_NO_PROBE || \
    FN_OPEN_KEEP != FN_OPEN_FLAG_KEEP
#error Public and wire Open flag values must be mapped explicitly
#endif
    *wire_flags = flags & (FN_OPEN_FOLLOW_REDIR | FN_OPEN_BODY_UNKNOWN |
                           FN_OPEN_ALLOW_EVICT | FN_OPEN_STREAM_NO_PROBE | FN_OPEN_KEEP);

    return FN_OK;
}

uint8_t fn_open(fn_handle_t *handle,
                uint8_t method,
                const char *url,
                uint8_t flags)
{
    uint16_t req_len;
    uint8_t open_flags;
    uint8_t result;

    result = fn_open_prepare(handle, url, flags, &open_flags);
    if (result != FN_OK) {
        return result;
    }

    req_len = fn_build_open_packet(_fn_req_buf, method, open_flags, url);
    if (req_len == 0) {
        return FN_ERR_INVALID;
    }

    return fn_open_exchange(handle, req_len);
}
