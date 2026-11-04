#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

uint16_t fn_palmos_read(uint16_t offset)
{
    return _fn_palmos_cs1 ? FN_PALMOS_RD(offset) : 0;
}
