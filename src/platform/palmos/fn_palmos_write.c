#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

void fn_palmos_write(uint16_t reg, uint16_t value)
{
    if (_fn_palmos_cs1) FN_PALMOS_WR(reg, value);
}
