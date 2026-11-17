#include "../service.h"

SERVICE(NioLibRawCall, (UInt16 refNum, UInt32 token, UInt16 device, UInt16 command, const void * payload, UInt16 payload_length, void * reply, UInt16 reply_capacity, fn_raw_response_t * response, UInt16 *resultP),
        (refNum, token, device, command, payload, payload_length, reply, reply_capacity, response, resultP))
{
    Err err;
    if (!resultP || device > 255 || command > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    /* Raw NetworkService commands would bypass owned-handle bookkeeping. */
    if (device == FN_DEVICE_NETWORK) *resultP = FN_ERR_UNSUPPORTED;
    else *resultP = fn_raw_call(device, command, payload, payload_length,
                               reply, reply_capacity, response);
    }
    nl_leave();
    return errNone;
}
