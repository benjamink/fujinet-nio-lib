#include "fujinet-nio.h"
#include "fn_protocol.h"
uint8_t fn_fuji_hsio_call(uint8_t, uint8_t *, uint8_t, uint8_t *);
uint8_t fn_fuji_set_hsio(uint8_t index)
{
    uint8_t req[2], confirmed = 0, result;
    if (index > 9 && index != FN_FUJI_HSIO_OFF) return FN_ERR_INVALID;
    req[0] = 1; req[1] = index;
    result = fn_fuji_hsio_call(FN_FUJI_CMD_SET_HSIO, req, 2, &confirmed);
    return result != FN_OK ? result : confirmed == index ? FN_OK : FN_ERR_IO;
}
