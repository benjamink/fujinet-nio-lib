#include "../service.h"
#include "fn_legacy_appkey.h"

SERVICE(NioLibLegacySetAppkeyDetails, (UInt16 refNum, UInt32 token, UInt16 creator_id, UInt16 app_id, UInt16 keysize, UInt16 *resultP),
        (refNum, token, creator_id, app_id, keysize, resultP))
{
    Err err;
    if (!resultP || app_id > 255) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    if (keysize > SIZE_256) { nl_leave(); return nioLibErrParam; }
    fuji_set_appkey_details(creator_id, app_id, (enum AppKeySize)keysize);
    *resultP = FN_OK;
    }
    nl_leave();
    return errNone;
}
