#include "fujinet-nio.h"
#include "fn_protocol.h"
uint8_t fn_fuji_hsio_call(uint8_t, uint8_t *, uint8_t, uint8_t *);
uint8_t fn_fuji_get_hsio(uint8_t *index)
{
    uint8_t req[1] = {1};
    if (!index) return FN_ERR_INVALID;
    return fn_fuji_hsio_call(FN_FUJI_CMD_GET_HSIO, req, 1, index);
}
