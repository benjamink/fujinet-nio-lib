/* Find the Springboard module and check its CS1 registers. */
#include <PalmOS.h>
#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

#ifndef FN_PALMOS_HOST_TEST
/* Handspring card attributes (HsExt.h) through the HsSelector trap. */
Err HsCardAttrGet(UInt16 cardNo, UInt16 attr, void *valueP)
    __attribute__((__callseq__("move.w #6,-(%%sp); trap #15; dc.w 0xA349")));
#endif

enum { hsAttrRemovable = 0, hsAttrHwInstalled = 1, hsAttrCsBase = 3, hsAttrCsSize = 4 };

uint32_t fn_palmos_open(void)
{
    uint16_t card;
    uint8_t removable, installed;
    uint32_t base, size;

    if (_fn_palmos_cs1) return _fn_palmos_cs1;
    for (card = 1; card < 4; card++) {
        removable = installed = 0;
        if (HsCardAttrGet(card, hsAttrRemovable, &removable) ||
            HsCardAttrGet(card, hsAttrHwInstalled, &installed) || !removable || !installed ||
            HsCardAttrGet(card, hsAttrCsBase, &base) || HsCardAttrGet(card, hsAttrCsSize, &size))
            continue;
        /* CS1 sits right above the card's CS0 window. */
        _fn_palmos_cs1 = base + size;
        if (FN_PALMOS_RD(SBN_R_MAGIC) != SBN_MAGIC_VALUE ||
            FN_PALMOS_RD(SBN_R_VERSION) != SBN_ABI_VERSION) {
            _fn_palmos_cs1 = 0;
            continue;
        }
        _fn_palmos_cs0 = base;
        _fn_palmos_card = card;
        /* Carry on from the firmware's last reply so a stale one never matches. */
        _fn_palmos_seq = (uint8_t)FN_PALMOS_RD(SBN_R_RESP_SEQ);
        return _fn_palmos_cs1;
    }
    return 0;
}
