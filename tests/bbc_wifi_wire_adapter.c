#include "fn_raw.h"
#include "fujinet-nio.h"
/* The real OSWORD adapter requires both output pointers, even if callers
 * do not need the response length. Preserve that contract in these vectors. */
uint8_t fn_bbc_device_call_raw(uint8_t device, uint8_t command,
    const uint8_t *payload, uint16_t len, uint8_t *reply, uint16_t capacity,
    uint8_t *status, uint16_t *reply_len)
{
    fn_raw_response_t response;
    uint8_t result;
    if (!status || !reply_len) return FN_ERR_INVALID;
    result=fn_raw_call(device,command,payload,len,reply,capacity,&response);
    *status=response.status; *reply_len=response.payload_length;
    return result;
}
