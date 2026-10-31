#include "fujinet-nio.h"
#include "fn_protocol.h"
#include "fn_bbc_internal.h"
uint8_t fn_fuji_hsio_call(uint8_t command, uint8_t *req, uint8_t length, uint8_t *index)
{
    uint8_t reply[2], status = 0, result;
    uint16_t len = 0;
    result = fn_bbc_device_call_raw(FN_DEVICE_FUJI, command, req, length, reply, 2, &status, &len);
    if (result != FN_OK) return result;
    if (status == 8) return FN_ERR_UNSUPPORTED;
    if (status == 2) return FN_ERR_INVALID;
    if (status || len != 2 || reply[0] != 1 || (reply[1] > 9 && reply[1] != 0xff)) return FN_ERR_IO;
    if (index) *index = reply[1];
    return FN_OK;
}
