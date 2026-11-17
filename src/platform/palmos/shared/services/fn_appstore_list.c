#include "../service.h"

SERVICE(NioLibAppstoreList, (UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, UInt16 start_index, uint8_t * key_data, UInt16 key_data_capacity, fn_appstore_list_t * out, UInt16 *resultP),
        (refNum, token, io, namespace_name, start_index, key_data, key_data_capacity, out, resultP))
{
    Err err;
    if (!resultP) return nioLibErrParam;
    err = nl_enter(token); if (err) return err;
    {
    *resultP = fn_appstore_list(io, namespace_name, start_index, key_data, key_data_capacity, out);
    }
    nl_leave();
    return errNone;
}
