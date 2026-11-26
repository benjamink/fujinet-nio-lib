#include "fn_file_internal.h"

uint8_t fn_file_stat(fn_file_io_t *io, const char *uri, fn_file_stat_t *out)
{
    uint16_t at, length;
    uint8_t result;
    if (!out || !io || io->capacity < 20) return FN_ERR_INVALID;
    at = fn_file_prefix(io, uri, 0);
    if (!at) return FN_ERR_INVALID;
    result = fn_file_call(io, FN_CMD_FILE_STAT, at, 20, &length);
    if (result) return result;
    if (length != 20) return FN_ERR_IO;
    out->flags = io->buffer[1];
    out->size_bytes = FN_GET_LE32(io->buffer + 4);
    out->size_bytes_high = FN_GET_LE32(io->buffer + 8);
    out->mtime_unix = FN_GET_LE32(io->buffer + 12);
    out->mtime_unix_high = FN_GET_LE32(io->buffer + 16);
    return FN_OK;
}
