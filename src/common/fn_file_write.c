#include "fn_file_internal.h"

uint8_t fn_file_write(fn_file_io_t *io, const char *uri, uint32_t offset,
                      const uint8_t *data, uint16_t length, fn_file_write_t *out)
{
    uint16_t at, received, count;
    uint8_t result;
    if (!out || (!data && length) || !io || io->capacity < 10 ||
        length > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 9u)
        return FN_ERR_INVALID;
    at = fn_file_prefix(io, uri, (uint16_t)(6 + length));
    if (!at) return FN_ERR_INVALID;
    FN_PUT_LE32(io->buffer + at, offset);
    FN_PUT_LE16(io->buffer + at + 4, length);
    if (length) memcpy(io->buffer + at + 6, data, length);
    result = fn_file_call(io, FN_CMD_FILE_WRITE, (uint16_t)(at + 6 + length), 10, &received);
    if (result) return result;
    count = FN_GET_LE16(io->buffer + 8);
    if (received != 10 || FN_GET_LE32(io->buffer + 4) != offset || count > length)
        return FN_ERR_IO;
    out->offset = offset; out->bytes_written = count;
    return FN_OK;
}
