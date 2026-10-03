#include "fujinet-nio.h"

uint8_t fn_open_translated(fn_handle_t *handle, uint8_t method, const char *url, uint8_t flags,
                           uint8_t ttype, uint8_t tflags, const char *selector)
{
    (void)tflags; (void)selector;
    if (ttype != FN_TRANSLATE_NONE) return FN_ERR_UNSUPPORTED;
    return fn_open(handle, method, url, flags);
}
