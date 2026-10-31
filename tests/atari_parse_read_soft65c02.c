#include <stdint.h>
#include <string.h>
#include "fn_internal.h"

fn_parse_ctx_t _fn_parse_ctx;
uint8_t test_case, test_seen, test_result;
static uint8_t input[300], output[264];
uint8_t profile_read(void);

uint8_t fn_parse_response_header(void)
{
    if (test_case == 6) return FN_ERR_IO;
    _fn_parse_ctx.status = test_case == 5 ? FN_ERR_NOT_READY : 0;
    _fn_parse_ctx.data_offset = 5;
    _fn_parse_ctx.data_len = test_case == 1 ? 11 : (test_case == 3 ? 271 :
                              (test_case == 8 ? 267 : (test_case == 9 ? 268 : 272)));
    return 0;
}
void test_func(void)
{
    uint16_t i, n = (test_case == 4 || test_case == 10) ? 0 :
                    (test_case == 8 ? 255 : (test_case == 9 ? 256 : 260));
    uint8_t result, expected = 0;
    test_seen = test_case;
    test_result = 0xff;
    memset(&_fn_parse_ctx, 0, sizeof(_fn_parse_ctx));
    memset(input, 0, sizeof(input));
    memset(output, 0xa5, sizeof(output));
    _fn_parse_ctx.response = input;
    _fn_parse_ctx.data = test_case == 4 ? 0 : output + 1;
    _fn_parse_ctx.data_max = test_case == 2 ? 259 : 260;
    input[6] = test_case == 7 ? 0 : FN_READ_AVAILABLE;
    input[7] = 0x34; input[8] = 0x12; input[9] = 0x78; input[10] = 0x56;
    input[11] = 1; input[12] = 2; input[13] = 3; input[14] = 4;
    input[15] = (uint8_t)n; input[16] = (uint8_t)(n >> 8);
    for (i = 0; i < n; ++i) input[17 + i] = (uint8_t)(i ^ 0x5a);
    if (test_case == 1 || test_case == 2 || test_case == 3) expected = FN_ERR_INVALID;
    if (test_case == 5) expected = FN_ERR_NOT_READY;
    if (test_case == 6) expected = FN_ERR_IO;
    result = profile_read();
    if (result != expected) { test_result = 1; return; }
    if (!result) {
        if (_fn_parse_ctx.data_len != n || _fn_parse_ctx.handle != 0x5678 ||
            _fn_parse_ctx.offset_echo != 0x04030201UL ||
            _fn_parse_ctx.available != (test_case == 7 ? 0 : 0x1234)) { test_result = 2; return; }
        if (test_case != 4) for (i = 0; i < n; ++i)
            if (output[1 + i] != (uint8_t)(i ^ 0x5a)) { test_result = 3; return; }
    } else for (i = 0; i < sizeof(output); ++i)
        if (output[i] != 0xa5) { test_result = 4; return; }
    if (output[0] != 0xa5 || output[1 + n] != 0xa5) { test_result = 5; return; }
    test_result = 0;
}
