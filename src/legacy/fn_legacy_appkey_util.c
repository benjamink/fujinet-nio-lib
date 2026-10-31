#include <string.h>

#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_protocol.h"
#include "fn_raw.h"
#include "fn_legacy_appkey_internal.h"

#define FN_FILEPROTO_VERSION 1

uint16_t _fn_legacy_appkey_capacity(void)
{
    return _fn_legacy_appkey_size == SIZE_256
        ? FN_LEGACY_APPKEY_SIZE_256
        : FN_LEGACY_APPKEY_DEFAULT_SIZE;
}

uint16_t _fn_legacy_file_uri_prefix(uint8_t *buf, const char *uri, uint16_t uri_len)
{
    buf[0] = FN_FILEPROTO_VERSION;
    FN_PUT_LE16(&buf[1], uri_len);
    memcpy(&buf[3], uri, uri_len);
    return (uint16_t)(3 + uri_len);
}
