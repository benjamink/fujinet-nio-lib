#include <stdio.h>
#include <string.h>
#include "fujinet-nio.h"
#include "fn_internal.h"

static int fails;
#define CHECK(c) \
    do { \
        if (!(c)) { \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            ++fails; \
        } \
    } while (0)

int main(void)
{
    static uint8_t a[FN_MAX_PACKET_SIZE];
    static uint8_t b[FN_MAX_PACKET_SIZE];
    uint16_t la;
    uint16_t lb;
    uint16_t base;
    uint8_t saved;
    const char *url = "https://imgs.xkcd.com/comics/x.png";
    const char *sel = "fmt=ilbm,w=624,h=190,colors=12,base=4,par=1:2";

    la = fn_build_open_packet(a, FN_METHOD_GET, 0x02, url);
    lb = fn_build_open_packet_ext(b, FN_METHOD_GET, 0x02, url, FN_TRANSLATE_NONE, 0, NULL);
    /* NONE == legacy bytes */
    CHECK(la == lb);
    CHECK(memcmp(a, b, la) == 0);

    lb = fn_build_open_packet_ext(b, FN_METHOD_GET, 0x02, url, FN_TRANSLATE_IMAGE, 0, sel);
    CHECK(lb == (uint16_t)(la + 4 + 1 + 1 + 2 + strlen(sel)));
    /* header length field */
    CHECK((b[2] | (b[3] << 8)) == lb);

    /* checksum: the stored byte matches the library's calculation (which skips offset 4) */
    saved = b[FN_CHECKSUM_OFFSET];
    CHECK(saved == fn_calc_packet_checksum(b, lb));
    /* the checksum byte must not affect the sum */
    b[FN_CHECKSUM_OFFSET] = (uint8_t)(saved ^ 0xFF);
    CHECK(saved == fn_calc_packet_checksum(b, lb));
    b[FN_CHECKSUM_OFFSET] = saved;

    /* ext block starts where legacy packet ended */
    base = la;
    CHECK(b[base] == 1);
    CHECK(b[base + 1] == 0);
    CHECK(b[base + 2] == 0);
    CHECK(b[base + 3] == 0);
    CHECK(b[base + 4] == FN_TRANSLATE_IMAGE);
    CHECK(b[base + 5] == 0);
    CHECK((b[base + 6] | (b[base + 7] << 8)) == (int)strlen(sel));
    CHECK(memcmp(b + base + 8, sel, strlen(sel)) == 0);

    /* overflow returns 0 */
    {
        static char big[FN_MAX_PACKET_SIZE];

        memset(big, 'a', sizeof big - 1);
        CHECK(fn_build_open_packet_ext(b, FN_METHOD_GET, 0, url, FN_TRANSLATE_IMAGE, 0, big) == 0);
    }

    if (fails != 0) {
        printf("open_ext_wire_test: %d failures\n", fails);
        return 1;
    }
    printf("open_ext_wire_test: OK\n");
    return 0;
}
