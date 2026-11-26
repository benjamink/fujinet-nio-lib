#include "../client_private.h"
#include "fn_legacy_appkey.h"
void fuji_set_appkey_details(uint16_t creator_id, uint8_t app_id, enum AppKeySize keysize)
{
    nl_client_key_creator = creator_id;
    nl_client_key_app = app_id;
    nl_client_key_size = (UInt16)keysize;
}
