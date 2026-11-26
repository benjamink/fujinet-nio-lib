#include "../client_private.h"

uint8_t fn_file_list_pages(fn_file_io_t * io, const char * uri, const fn_list_pages_req_t * req, fn_list_pages_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibFileListPages(nl_client_ref, nl_client_token,
        io, uri, req, out, &nl_client_result_code));
}
