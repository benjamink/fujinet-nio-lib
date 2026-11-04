#include <string.h>
#include "fn_bbc_internal.h"
#include "fn_raw.h"

uint8_t fn_ready(fn_ready_entry_t *entries, uint8_t max_entries, uint8_t *count)
{
    uint8_t request=1, reply[85], block[16], result, i, j, flags;
    uint16_t firmware_handle, available, remote;
    fn_raw_response_t response;
    if (!_fn_initialized || !count || (!entries && max_entries)) return FN_ERR_INVALID;
    *count=0;
    result=fn_raw_call(0xFD,8,&request,1,reply,sizeof(reply),&response);
    if (result!=FN_OK) return result;
    if (response.status) return response.status<=8 ? response.status : FN_ERR_UNKNOWN;
    if (response.payload_length<5 || reply[0]!=1 ||
        response.payload_length!=(uint16_t)(5+5*(uint16_t)reply[4])) return FN_ERR_IO;
    for (i=0;i<FN_MAX_SESSIONS && *count<max_entries;++i) if (_fn_sessions[i].active) {
        memset(block,0,sizeof(block)); block[0]=8; block[2]=(uint8_t)_fn_sessions[i].handle;
        if (fn_bbc_osword78(block)!=0) return FN_ERR_IO;
        firmware_handle=FN_READ_LE16(block,3);
        available=FN_READ_LE16(block,5); flags=block[7] ? FN_READY_READ : 0;
        for (j=0;j<reply[4];++j) if (FN_READ_LE16(reply,5+5*j)==firmware_handle) {
            flags|=reply[7+5*j]; remote=FN_READ_LE16(reply,8+5*j);
            if (remote>(uint16_t)(65535u-available)) available=65535u;
            else available=(uint16_t)(available+remote);
            break;
        }
        if (flags) {
            entries[*count].handle=_fn_sessions[i].handle;
            entries[*count].flags=flags; entries[*count].available=available;
            ++*count;
        }
    }
    return FN_OK;
}
