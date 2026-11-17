#include <string.h>
#include "fn_disk_internal.h"

uint8_t fn_disk_inspect(fn_service_io_t *io, const char *uri, uint8_t type,
                        uint16_t sector_size_hint, fn_disk_inspection_t *inspection)
{
    size_t uri_length;
    uint16_t limit, length = 0, boot;
    uint8_t result;
    if (!io || !io->buffer || io->capacity < 10 || !uri || !uri[0] || !inspection)
        return FN_ERR_INVALID;
    uri_length = strlen(uri);
    if (uri_length > io->capacity - 9u ||
        uri_length > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 9u) return FN_ERR_INVALID;
    limit = (uint16_t)(io->capacity - 10);
    if (limit > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 1u - 10u)
        limit = FN_MAX_PACKET_SIZE - FN_HEADER_SIZE - 1u - 10u;
    if (limit > FN_DISK_INSPECT_BOOT_BYTES) limit = FN_DISK_INSPECT_BOOT_BYTES;
    io->buffer[0] = FN_DISK_PROTOCOL_VERSION;
    io->buffer[1] = 0;
    io->buffer[2] = type;
    FN_PUT_LE16(io->buffer + 3, sector_size_hint);
    FN_PUT_LE16(io->buffer + 5, limit);
    FN_PUT_LE16(io->buffer + 7, uri_length);
    memcpy(io->buffer + 9, uri, uri_length);
    result = fn_disk_call(FN_DISK_CMD_INSPECT, io->buffer,
        (uint16_t)(9 + uri_length), io->buffer, io->capacity, &length);
    if (result) return result;
    if (length < 10 || io->buffer[0] != FN_DISK_PROTOCOL_VERSION) return FN_ERR_IO;
    boot = FN_GET_LE16(io->buffer + 8);
    if (boot > limit || (uint32_t)10 + boot != length) return FN_ERR_IO;
    memset(inspection, 0, sizeof(*inspection));
    inspection->media.flags = FN_DISK_FLAG_MOUNTED | FN_DISK_FLAG_READONLY;
    inspection->media.type = io->buffer[1];
    inspection->media.sector_size = FN_GET_LE16(io->buffer + 2);
    inspection->media.sector_count = FN_GET_LE32(io->buffer + 4);
    inspection->boot_length = boot;
    if (boot) memcpy(inspection->boot_bytes, io->buffer + 10, boot);
    return FN_OK;
}
