#include "../service.h"
#include "fn_legacy_appkey.h"

SERVICE(NioLibLegacyWriteAppkey, (UInt16 refNum, UInt32 token, UInt16 key_id, UInt16 count, uint8_t * data, UInt16 *resultP),
        (refNum, token, key_id, count, data, resultP))
{
    Err err;
    if (!resultP || key_id > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fuji_write_appkey(key_id, count, data) ? FN_OK : FN_ERR_IO;
    }
    nl_leave();
    return errNone;
}
