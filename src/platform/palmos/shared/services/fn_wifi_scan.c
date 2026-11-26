#include "../service.h"

SERVICE(NioLibWifiScan, (UInt16 refNum, UInt32 token, UInt16 offset, UInt16 limit, fn_wifi_scan_record_t * records, UInt16 capacity, uint8_t * count, uint8_t * more, uint8_t * response_buffer, UInt16 response_capacity, UInt16 *resultP),
        (refNum, token, offset, limit, records, capacity, count, more, response_buffer, response_capacity, resultP))
{
    Err err;
    if (!resultP || limit > 255 || capacity > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_wifi_scan(offset, limit, records, capacity, count, more, response_buffer, response_capacity);
    }
    nl_leave();
    return errNone;
}
