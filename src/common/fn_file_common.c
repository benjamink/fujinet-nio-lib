#include "fn_file_internal.h"

uint16_t fn_file_prefix(fn_file_io_t *io, const char *uri, uint16_t tail)
{
    size_t n;
    if (!io || !io->buffer || !uri || !uri[0] || io->capacity < 4 ||
        tail > io->capacity - 3) return 0;
    n = strlen(uri);
    if (n > io->capacity - 3u - tail ||
        n > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 3u - tail) return 0;
    io->buffer[0] = 1;
    FN_PUT_LE16(io->buffer + 1, n);
    memcpy(io->buffer + 3, uri, n);
    return (uint16_t)(3 + n);
}

uint8_t fn_file_call(fn_file_io_t *io, uint8_t command, uint16_t length,
                     uint16_t minimum, uint16_t *received)
{
    fn_raw_response_t raw;
    uint8_t result = fn_raw_call(FN_DEVICE_FILE, command, io->buffer, length,
                                 io->buffer, io->capacity, &raw);
    if (result) return result;
    if (raw.status) return raw.status;
    if (raw.payload_length < minimum || io->buffer[0] != 1 ||
        io->buffer[2] || io->buffer[3]) return FN_ERR_IO;
    *received = raw.payload_length;
    return FN_OK;
}
