#include "../client_private.h"

uint8_t fn_raw_call(uint8_t device, uint8_t command, const void * payload, uint16_t payload_length, void * reply, uint16_t reply_capacity, fn_raw_response_t * response)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibRawCall(nl_client_ref, nl_client_token,
        device, command, payload, payload_length, reply, reply_capacity, response, &nl_client_result_code));
}
