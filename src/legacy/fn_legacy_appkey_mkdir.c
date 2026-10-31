#include "fujinet-nio.h"
#include "fn_protocol.h"
#include "fn_raw.h"
#include "fn_legacy_appkey_internal.h"

static const char k_fuji_dir[] = "persist:///FujiNet";

uint8_t _fn_legacy_file_mkdir_fuji(void)
{
    uint16_t off;
    fn_raw_response_t raw;

    off = _fn_legacy_file_uri_prefix(
        _fn_legacy_appkey_transfer,
        k_fuji_dir,
        (uint16_t)(sizeof(k_fuji_dir) - 1));
    _fn_legacy_appkey_transfer[off++] = 0x03; /* parents + exist_ok */

    if (fn_raw_call(FN_DEVICE_FILE,
                    FN_CMD_FILE_MKDIR,
                    _fn_legacy_appkey_transfer,
                    off,
                    _fn_legacy_appkey_transfer,
                    4,
                    &raw) != FN_OK) {
        return FN_ERR_IO;
    }
    return raw.status;
}
