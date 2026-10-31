#include <stdint.h>
#include <string.h>
#include "../src/legacy/fn_legacy_appkey_internal.h"

uint8_t test_case, test_seen, test_result;
static char guarded[34];
uint16_t profile_uri(uint8_t key, char *uri, uint16_t capacity);

void test_func(void)
{
    uint16_t result;
    uint8_t i;
    test_seen = test_case;
    test_result = 0xff;
    memset(guarded, 0xa5, sizeof(guarded));
    _fn_legacy_appkey_creator_id = test_case == 3 ? 0xffff : 0x12ab;
    _fn_legacy_appkey_app_id = test_case == 3 ? 0 : 0xcd;
    result = profile_uri(test_case == 3 ? 0x10 : 0xef, guarded + 1,
                         test_case == 1 ? 31 : (test_case == 2 ? 256 : 32));
    if (test_case == 1) {
        if (result != 0) { test_result = 1; return; }
        for (i = 0; i < sizeof(guarded); ++i)
            if ((uint8_t)guarded[i] != 0xa5) { test_result = 2; return; }
    } else {
        const char *expected = test_case == 3 ? "persist:///FujiNet/ffff0010.key" : "persist:///FujiNet/12abcdef.key";
        if (result != 31 || strcmp(guarded + 1, expected)) { test_result = 3; return; }
        if ((uint8_t)guarded[0] != 0xa5 || (uint8_t)guarded[33] != 0xa5) { test_result = 4; return; }
    }
    test_result = 0;
}
