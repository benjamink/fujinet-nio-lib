#include "fn_bbc_internal.h"
#include "fn_raw.h"

/* The resident ROM owns FujiBus framing. No library packet workspace. */
uint8_t fn_raw_call(uint8_t device, uint8_t command, const void *payload,
                    uint16_t payload_length, void *reply, uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    if (!response || (payload_length && !payload) || (reply_capacity && !reply))
        return FN_ERR_INVALID;
    response->status = FN_ERR_INTERNAL;
    response->payload_length = 0;
    return fn_bbc_device_call_raw(device, command, payload, payload_length,
        reply, reply_capacity, &response->status, &response->payload_length);
}
