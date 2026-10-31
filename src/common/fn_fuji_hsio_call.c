#include "fujinet-nio.h"
#include "fn_protocol.h"
#include "fn_raw.h"
uint8_t fn_fuji_hsio_call(uint8_t command, uint8_t *req, uint8_t length, uint8_t *index)
{
    uint8_t reply[2], result;
    fn_raw_response_t r;
    result = fn_raw_call(FN_DEVICE_FUJI, command, req, length, reply, sizeof(reply), &r);
    if (result != FN_OK) return result;
    if (r.status == 8) return FN_ERR_UNSUPPORTED;
    if (r.status == 2) return FN_ERR_INVALID;
    if (r.status || r.payload_length != 2 || reply[0] != 1 || (reply[1] > 9 && reply[1] != 0xff)) return FN_ERR_IO;
    if (index) *index = reply[1];
    return FN_OK;
}
