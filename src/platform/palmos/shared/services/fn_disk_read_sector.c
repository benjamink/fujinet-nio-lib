#include "../service.h"

SERVICE(NioLibDiskReadSector, (UInt16 refNum, UInt32 token, UInt16 slot, UInt32 lba, uint8_t * data, UInt16 data_capacity, uint16_t * data_length, UInt16 *resultP),
        (refNum, token, slot, lba, data, data_capacity, data_length, resultP))
{
    Err err;
    if (!resultP || slot > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_disk_read_sector(slot, lba, data, data_capacity, data_length);
    }
    nl_leave();
    return errNone;
}
