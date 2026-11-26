#include "../service.h"

SERVICE(NioLibDiskWriteSector, (UInt16 refNum, UInt32 token, UInt16 slot, UInt32 lba, const uint8_t * data, UInt16 data_length, UInt16 *resultP),
        (refNum, token, slot, lba, data, data_length, resultP))
{
    Err err;
    if (!resultP || slot > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_disk_write_sector(slot, lba, data, data_length);
    }
    nl_leave();
    return errNone;
}
