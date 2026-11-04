#include <conio.h>
#include "fujinet-nio.h"
static unsigned char buffer[128];
int main(void)
{
    fn_file_io_t io;
    fn_list_pages_req_t req;
    fn_list_pages_t out;
    unsigned char i;
    io.buffer=buffer; io.capacity=sizeof(buffer);
    req.page_size=1; req.max_payload=120; req.flags=FN_LIST_DIRS_FIRST;
    req.max_name=40; req.filter="*MiNeR*";
    for (i=0;i<2;++i) {
        req.start_page=i;
        if (fn_file_list_pages(&io,"sd0:/games",&req,&out)!=FN_OK ||
            out.total!=2 || out.start_page!=i || out.page_count!=1 ||
            out.pages_len!=4 || out.pages[3]!=i) {
            cputs("PAGES FAIL"); return 1;
        }
    }
    req.filter=0;
    if (fn_file_list_pages(&io,"sd0:/games",&req,&out)!=FN_ERR_IO) {
        cputs("SHORT FAIL"); return 1;
    }
    cputs("PAGES OK"); return 0;
}
