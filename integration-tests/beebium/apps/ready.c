#include <conio.h>
#include "fujinet-nio.h"
int main(void) {
    fn_handle_t h; uint8_t buf[2],flags,count; uint16_t n;
    fn_ready_entry_t entry;
    if(fn_init() || fn_open(&h,0,"tcp://example.com:7777",0))return 1;
    if(fn_read(h,0,buf,2,&n,&flags) || n!=2 || !(flags&FN_READ_AVAILABLE) || fn_read_available()!=1) {
        cputs("AVAILABLE FAIL");return 1;
    }
    if(fn_ready(&entry,1,&count) || count!=1 || entry.handle!=h ||
        entry.available!=1 || !(entry.flags&FN_READY_READ)) {
        cputs("READY FAIL");return 1;
    }
    fn_close(h);cputs("READY OK");return 0;
}
