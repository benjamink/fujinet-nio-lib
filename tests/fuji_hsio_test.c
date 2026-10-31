#include <assert.h>
#include <stdint.h>
#include "fn_raw.h"
#include "fujinet-nio.h"
static uint8_t last, invalid, status, length = 2;
uint8_t fn_raw_call(uint8_t dev, uint8_t cmd, const void *data, uint16_t n,
                    void *reply, uint16_t cap, fn_raw_response_t *r)
{
    const uint8_t *p = data; uint8_t *out = reply;
    assert(dev == 0x70 && cap == 2 && p[0] == 1);
    if (cmd == 3) { assert(n == 2); last = p[1]; }
    else { assert(cmd == 2 && n == 1); }
    out[0] = invalid ? 2 : 1; out[1] = last;
    r->status = status; r->payload_length = length;
    return FN_OK;
}
int main(void)
{
    uint8_t got = 77;
    assert(fn_fuji_set_hsio(4) == FN_OK && last == 4);
    assert(fn_fuji_get_hsio(&got) == FN_OK && got == 4);
    assert(fn_fuji_set_hsio(255) == FN_OK && last == 255);
    assert(fn_fuji_set_hsio(10) == FN_ERR_INVALID);
    assert(fn_fuji_get_hsio(0) == FN_ERR_INVALID);
    invalid = 1; got = 77;
    assert(fn_fuji_get_hsio(&got) == FN_ERR_IO && got == 77);
    invalid = 0; length = 1;
    assert(fn_fuji_get_hsio(&got) == FN_ERR_IO);
    length = 2; status = 8;
    assert(fn_fuji_get_hsio(&got) == FN_ERR_UNSUPPORTED);
    return 0;
}
