#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

void fn_palmos_close(void)
{
    _fn_palmos_cs0 = _fn_palmos_cs1 = 0;
    _fn_palmos_card = 0;
}
