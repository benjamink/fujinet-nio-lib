#include "../service.h"
#include "fn_legacy_appkey.h"

SERVICE(NioLibLegacyReadAppkeyBuffered, (UInt16 refNum, UInt32 token, UInt16 key_id, uint16_t * count, uint8_t * data, uint8_t * scratch, UInt16 scratch_size, UInt16 *resultP),
        (refNum, token, key_id, count, data, scratch, scratch_size, resultP))
{
    Err err;
    if (!resultP || key_id > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fuji_read_appkey_buffered(key_id, count, data, scratch, scratch_size) ? FN_OK : FN_ERR_IO;
    }
    nl_leave();
    return errNone;
}
