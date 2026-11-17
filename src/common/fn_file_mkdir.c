#include "fn_file_internal.h"

uint8_t fn_file_mkdir(fn_file_io_t *io, const char *uri, uint8_t flags)
{
    uint16_t at, length;
    uint8_t result;
    if (flags & ~(FN_FILE_MKDIR_PARENTS | FN_FILE_MKDIR_EXIST_OK)) return FN_ERR_INVALID;
    at = fn_file_prefix(io, uri, 1);
    if (!at) return FN_ERR_INVALID;
    io->buffer[at++] = flags;
    result = fn_file_call(io, FN_CMD_FILE_MKDIR, at, 4, &length);
    if (result) return result;
    return length == 4 ? FN_OK : FN_ERR_IO;
}
