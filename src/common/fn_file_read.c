#include "fn_file_internal.h"

uint8_t fn_file_read(fn_file_io_t *io, const char *uri, uint32_t offset,
                     uint8_t *data, uint16_t capacity, fn_file_read_t *out)
{
    uint16_t at, length, count;
    uint8_t result;
    if (!out || !data || !capacity || !io || io->capacity < 10 ||
        capacity > io->capacity - 10u ||
        capacity > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 1u - 10u)
        return FN_ERR_INVALID;
    at = fn_file_prefix(io, uri, 6);
    if (!at) return FN_ERR_INVALID;
    FN_PUT_LE32(io->buffer + at, offset);
    FN_PUT_LE16(io->buffer + at + 4, capacity);
    result = fn_file_call(io, FN_CMD_FILE_READ, (uint16_t)(at + 6), 10, &length);
    if (result) return result;
    count = FN_GET_LE16(io->buffer + 8);
    if (FN_GET_LE32(io->buffer + 4) != offset || count > capacity ||
        (uint32_t)10 + count != length) return FN_ERR_IO;
    if (count) memcpy(data, io->buffer + 10, count);
    out->offset = offset; out->flags = io->buffer[1]; out->bytes_read = count;
    return FN_OK;
}
