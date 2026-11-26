#include "../client_private.h"

uint8_t fn_open_headers(fn_handle_t * handle, uint8_t method, const char * url, uint8_t flags, const fn_http_header_t * headers, uint16_t header_count, uint32_t body_len)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibOpenHeaders(nl_client_ref, nl_client_token,
        handle, method, url, flags, headers, header_count, body_len, &nl_client_result_code));
}
