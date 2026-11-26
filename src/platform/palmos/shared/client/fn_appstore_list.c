#include "../client_private.h"

uint8_t fn_appstore_list(fn_appstore_io_t * io, const char * namespace_name, uint16_t start_index, uint8_t * key_data, uint16_t key_data_capacity, fn_appstore_list_t * out)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibAppstoreList(nl_client_ref, nl_client_token,
        io, namespace_name, start_index, key_data, key_data_capacity, out, &nl_client_result_code));
}
