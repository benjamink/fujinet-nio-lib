#include "../client_private.h"

uint8_t fn_wifi_scan(uint16_t offset, uint8_t limit, fn_wifi_scan_record_t * records, uint8_t capacity, uint8_t * count, uint8_t * more, uint8_t * response_buffer, uint16_t response_capacity)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibWifiScan(nl_client_ref, nl_client_token,
        offset, limit, records, capacity, count, more, response_buffer, response_capacity, &nl_client_result_code));
}
