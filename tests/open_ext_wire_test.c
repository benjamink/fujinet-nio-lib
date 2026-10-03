#include <stdio.h>
#include <string.h>
#include "fujinet-nio.h"
#include "fn_internal.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main(void)
{
    static uint8_t a[FN_MAX_PACKET_SIZE], b[FN_MAX_PACKET_SIZE];
    uint16_t la, lb, base;
    const char *url = "https://imgs.xkcd.com/comics/x.png";
    const char *sel = "w=624,h=190,colors=12,base=4,par=1:2";

    la = fn_build_open_packet(a, FN_METHOD_GET, 0x02, url);
    lb = fn_build_open_packet_ext(b, FN_METHOD_GET, 0x02, url, FN_TRANSLATE_NONE, 0, NULL);
    CHECK(la == lb && memcmp(a, b, la) == 0);          /* NONE == legacy bytes */

    lb = fn_build_open_packet_ext(b, FN_METHOD_GET, 0x02, url, FN_TRANSLATE_IMAGE, 0, sel);
    CHECK(lb == (uint16_t)(la + 4 + 1 + 1 + 2 + strlen(sel)));
    base = la;                                          /* ext block starts where legacy packet ended */
    CHECK(b[base] == 1 && b[base+1] == 0 && b[base+2] == 0 && b[base+3] == 0);
    CHECK(b[base+4] == FN_TRANSLATE_IMAGE);
    CHECK(b[base+5] == 0);
    CHECK((b[base+6] | (b[base+7] << 8)) == (int)strlen(sel));
    CHECK(memcmp(b + base + 8, sel, strlen(sel)) == 0);

    {   /* overflow returns 0 */
        static char big[FN_MAX_PACKET_SIZE];
        memset(big, 'a', sizeof big - 1);
        CHECK(fn_build_open_packet_ext(b, FN_METHOD_GET, 0, url, FN_TRANSLATE_IMAGE, 0, big) == 0);
    }
    printf(fails ? "open_ext_wire_test: %d failures\n" : "open_ext_wire_test: OK\n", fails);
    return fails != 0;
}
