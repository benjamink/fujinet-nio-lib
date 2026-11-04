/* fujinet-nio-lib transport for Palm OS: raw FujiBus packets through the
 * Springboard module's CS1 link. The firmware does the SLIP framing to NIO. */
#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "fn_palmos_internal.h"

uint8_t fn_transport_init(void)
{
    return fn_palmos_open() ? FN_OK : FN_ERR_NOT_FOUND;
}

/* Closing the link makes the next fn_init() look for the module again. */
void fn_transport_close(void)
{
    fn_palmos_close();
}

uint8_t fn_transport_ready(void)
{
    return _fn_palmos_cs1 ? 1 : 0;
}

uint8_t fn_transport_exchange_buffers(const uint8_t *request, uint16_t request_length,
                                      uint8_t *response, uint16_t response_capacity,
                                      uint16_t *response_length)
{
    uint16_t got = 0;

    if (!response_length) return FN_ERR_INVALID;
    *response_length = 0;
    if (!_fn_palmos_cs1) return FN_ERR_NOT_FOUND;
    if (!request || !response) return FN_ERR_INVALID;
    switch (fn_palmos_exchange(request, request_length, response, response_capacity, &got)) {
    case SBN_RES_OK:
        *response_length = got;
        return FN_OK;
    case SBN_RES_NO_HOST: return FN_ERR_TRANSPORT;
    case SBN_RES_TIMEOUT:
    case SBN_RES_UNANSWERED:
    case FN_PALMOS_NO_ANSWER: return FN_ERR_TIMEOUT;
    case SBN_RES_BAD_LEN: return FN_ERR_INVALID;
    case SBN_RES_ABORTED: return FN_ERR_ABORTED;
    default: return FN_ERR_IO;
    }
}

uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request, _fn_transport_ctx.req_len,
                                         _fn_transport_ctx.response, _fn_transport_ctx.resp_max,
                                         &_fn_transport_ctx.resp_len);
}

const char *fn_platform_name(void)
{
    return "palmos-springboard";
}
