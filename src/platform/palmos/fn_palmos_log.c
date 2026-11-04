#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

/* Two characters per LOG word, high byte first; the newline rides in the
 * last word and a 0 character means none. */
void fn_palmos_log(const char *text)
{
    uint16_t hi, lo;

    if (!_fn_palmos_cs1) return;
    for (;;) {
        hi = (uint8_t)*text;
        if (!hi) {
            FN_PALMOS_WR(SBN_W_LOG, (uint16_t)('\n' << 8));
            return;
        }
        lo = (uint8_t)*++text;
        if (!lo) {
            FN_PALMOS_WR(SBN_W_LOG, (uint16_t)(hi << 8 | '\n'));
            return;
        }
        text++;
        FN_PALMOS_WR(SBN_W_LOG, (uint16_t)(hi << 8 | lo));
    }
}
