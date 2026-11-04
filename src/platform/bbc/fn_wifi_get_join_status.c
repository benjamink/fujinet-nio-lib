#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_protocol.h"
#include "fn_wifi_bbc.h"

uint8_t fn_wifi_get_join_status(uint8_t *state)
{
    uint8_t request[] = { FN_WIFI_PROTOCOL_VERSION }, response[16], result;
    uint16_t len = 0;
    if (!state) return FN_ERR_INVALID;
    result = fn_bbc_wifi_call(FN_WIFI_CMD_GET_JOIN_STATUS, request, 1, response, sizeof(response), &len);
    if (result != FN_OK) return result;
    if (len < 2 || response[0] != FN_WIFI_PROTOCOL_VERSION || response[1] > FN_WIFI_JOIN_FAILED)
        return FN_ERR_IO;
    *state = response[1];
    return FN_OK;
}
