#include "NioLibClient.h"
uint8_t fn_open_long(fn_handle_t *handle, uint8_t method, const char *url, uint8_t flags)
{
    return fn_open(handle, method, url, flags);
}
