#include <string.h>
#include "fn_legacy_appkey_internal.h"

static const char k_persist_prefix[] = "persist:///FujiNet/";
static const char k_hex[] = "0123456789abcdef";

static void hex8(char *out, uint8_t value)
{
    out[0] = k_hex[value >> 4];
    out[1] = k_hex[value & 0x0f];
}

uint16_t _fn_legacy_appkey_build_uri(uint8_t key_id, char *uri, uint16_t uri_capacity)
{
    /* The complete key name has a fixed length: 19-byte prefix, eight hex
       digits and '.key'. Use fixed offsets to avoid 16-bit index arithmetic
       and by-reference offset updates on 6502 targets. */
    if (uri_capacity < sizeof(k_persist_prefix) + 12) return 0;
    memcpy(uri, k_persist_prefix, sizeof(k_persist_prefix) - 1);
    hex8(uri + 19, (uint8_t)(_fn_legacy_appkey_creator_id >> 8));
    hex8(uri + 21, (uint8_t)_fn_legacy_appkey_creator_id);
    hex8(uri + 23, _fn_legacy_appkey_app_id);
    hex8(uri + 25, key_id);
    memcpy(uri + 27, ".key", 5);
    return 31;
}
