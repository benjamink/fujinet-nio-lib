#include <string.h>

#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

enum {
    FN_DISK_CMD_MOUNT = 0x01,
    FN_DISK_CMD_UNMOUNT = 0x02,
    FN_DISK_CMD_READ_SECTOR = 0x03,
    FN_DISK_CMD_WRITE_SECTOR = 0x04,
    FN_DISK_CMD_INFO = 0x05,
    FN_DISK_CMD_CLEAR_CHANGED = 0x06,
    FN_DISK_CMD_RESTORE_BOOT = 0x0A,
    FN_DISK_CMD_FLUSH = 0x0E,
    FN_DISK_CMD_INSPECT = 0x0F
};

enum {
    /* The fixed Info fields end after sectorCount. */
    FN_DISK_INFO_RESPONSE_SIZE = 12,
    /* Older peers omit lastError; newer peers may append it. */
    FN_DISK_INFO_RESPONSE_SIZE_WITH_LAST_ERROR = 13
};

/* Legacy DiskDevice calls are synchronous. Keep their large codec buffers out
 * of cc65 stack frames; explicit-context clients own separate scratch storage
 * instead, while other legacy builds retain function-local buffers. */
#if defined(__CC65__)
static uint8_t disk_request[FN_MAX_PACKET_SIZE];
static uint8_t disk_reply[FN_MAX_PACKET_SIZE];
#endif

static uint8_t disk_status(uint8_t status)
{
    switch (status) {
        case 0: return FN_OK;
        case 1: return FN_ERR_NOT_FOUND;
        case 2: return FN_ERR_INVALID;
        case 3: return FN_ERR_BUSY;
        case 4: return FN_ERR_NOT_READY;
        case 5: return FN_ERR_IO;
        case 6: return FN_ERR_TIMEOUT;
        case 7: return FN_ERR_INTERNAL;
        case 8: return FN_ERR_UNSUPPORTED;
        default: return FN_ERR_UNKNOWN;
    }
}

static uint16_t get_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put_u16le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint8_t parse_info(const uint8_t *reply, uint16_t length,
                          fn_disk_info_t *info)
{
    /*
     * The original DiskDevice Info response ended at sectorCount (12 bytes),
     * while newer responses may append the optional lastError byte (13 bytes).
     * Accepting both preserves compatibility with existing BBC/MS-DOS clients
     * and older NIO peers; this is not arbitrary size tolerance.
     */
    if (!info || (length != FN_DISK_INFO_RESPONSE_SIZE &&
                  length != FN_DISK_INFO_RESPONSE_SIZE_WITH_LAST_ERROR) ||
        reply[0] != FN_DISK_PROTOCOL_VERSION) {
        return FN_ERR_IO;
    }
    info->flags = reply[1];
    info->slot = reply[4];
    info->type = reply[5];
    info->sector_size = get_u16le(reply + 6);
    info->sector_count = get_u32le(reply + 8);
    info->last_error = (length == FN_DISK_INFO_RESPONSE_SIZE_WITH_LAST_ERROR)
                           ? reply[12]
                           : 0;
    return FN_OK;
}

static uint8_t fn_disk_error = FN_DISK_ERR_NONE;

/* The DiskError a failed response carries as {version, DiskError}. */
static uint8_t failure_disk_error(const uint8_t *payload, uint16_t length)
{
    if (payload != 0 && length == 2 && payload[0] == FN_DISK_PROTOCOL_VERSION) {
        return payload[1];
    }
    return FN_DISK_ERR_UNREPORTED;
}

uint8_t fn_disk_last_error(void)
{
    return fn_disk_error;
}

static uint8_t disk_call(uint8_t command, const void *request,
                         uint16_t request_length, void *reply,
                         uint16_t reply_capacity, uint16_t *reply_length)
{
    fn_raw_response_t response;
    uint8_t result = fn_raw_call(FN_DEVICE_DISK, command,
                                 request, request_length, reply,
                                 reply_capacity, &response);
    fn_disk_error = FN_DISK_ERR_UNREPORTED;
    if (result != FN_OK) return result;
    if (reply_length) *reply_length = response.payload_length;
    fn_disk_error = response.status == 0
        ? FN_DISK_ERR_NONE
        : failure_disk_error((const uint8_t *)reply, response.payload_length);
    return disk_status(response.status);
}

#if !defined(__CC65__)
static uint8_t context_parse_response(fn_disk_client_context_t *context,
                                      uint8_t device, uint8_t command,
                                      uint16_t response_length,
                                      uint16_t *data_offset,
                                      uint16_t *data_length,
                                      uint8_t *status)
{
    static const uint8_t field_sizes[8] = { 0, 1, 1, 1, 1, 2, 2, 4 };
    static const uint8_t field_counts[8] = { 0, 1, 2, 3, 4, 1, 2, 1 };
    uint8_t *response = context->packet_response;
    uint16_t offset = FN_HEADER_SIZE;
    uint8_t descriptor;
    uint8_t field_size;
    uint8_t field_count;

    if (response_length < FN_HEADER_SIZE || response[0] != device ||
        response[1] != command || get_u16le(response + 2) != response_length) {
        return FN_ERR_INVALID;
    }
    if (fn_calc_packet_checksum(response, response_length) != response[FN_CHECKSUM_OFFSET]) {
        return FN_ERR_IO;
    }

    descriptor = response[5];
    *status = FN_OK;
    if (descriptor != 0) {
        while ((descriptor & 0x80u) != 0) {
            if (offset >= response_length) return FN_ERR_INVALID;
            descriptor = response[offset++];
        }
        field_size = field_sizes[descriptor & 7u];
        field_count = field_counts[descriptor & 7u];
        if ((uint16_t)(offset + (uint16_t)field_size * field_count) >
            response_length) {
            return FN_ERR_INVALID;
        }
        if (field_count != 0) {
            *status = response[offset];
            offset = (uint16_t)(offset + (uint16_t)field_size * field_count);
        }
    }
    *data_offset = offset;
    *data_length = (uint16_t)(response_length - offset);
    return FN_OK;
}

static uint8_t context_disk_call(fn_disk_client_context_t *context,
                                 uint8_t command, uint16_t request_length,
                                 uint16_t *reply_length)
{
    uint16_t packet_length;
    uint16_t response_length = 0;
    uint16_t data_offset;
    uint16_t data_length;
    uint8_t status;
    uint8_t result;

    if (context == NULL) return FN_ERR_INVALID;
    context->last_error = FN_DISK_ERR_UNREPORTED;
    if (context->exchange == NULL ||
        request_length > FN_DISK_CONTEXT_PACKET_SIZE - FN_HEADER_SIZE) {
        return FN_ERR_INVALID;
    }
    packet_length = fn_build_header(context->packet_request, FN_DEVICE_DISK,
                                    command,
                                    (uint16_t)(FN_HEADER_SIZE + request_length));
    if (request_length != 0) {
        memcpy(context->packet_request + packet_length,
               context->codec_scratch, request_length);
        packet_length = (uint16_t)(packet_length + request_length);
    }
    context->packet_request[FN_CHECKSUM_OFFSET] =
        fn_calc_packet_checksum(context->packet_request, packet_length);

    result = context->exchange(context->exchange_context,
                               context->packet_request, packet_length,
                               context->packet_response,
                               sizeof(context->packet_response),
                               &response_length);
    if (result != FN_OK) return result;
    result = context_parse_response(context, FN_DEVICE_DISK, command,
                                    response_length, &data_offset,
                                    &data_length, &status);
    if (result != FN_OK) return result;
    if (data_length > sizeof(context->codec_scratch)) return FN_ERR_IO;
    if (data_length != 0) {
        memcpy(context->codec_scratch,
               context->packet_response + data_offset, data_length);
    }
    if (reply_length != NULL) *reply_length = data_length;
    context->last_error = status == 0
        ? FN_DISK_ERR_NONE
        : failure_disk_error(context->codec_scratch, data_length);
    return disk_status(status);
}

uint8_t fn_disk_context_init(fn_disk_client_context_t *context,
                             fn_disk_exchange_fn exchange,
                             void *exchange_context)
{
    if (context == NULL || exchange == NULL) return FN_ERR_INVALID;
    memset(context, 0, sizeof(*context));
    context->exchange = exchange;
    context->exchange_context = exchange_context;
    return FN_OK;
}

uint8_t fn_disk_last_error_context(const fn_disk_client_context_t *context)
{
    return context != NULL ? context->last_error : FN_DISK_ERR_UNREPORTED;
}

uint8_t fn_disk_mount_context(fn_disk_client_context_t *context, uint8_t slot,
                              const char *uri, uint8_t readonly, uint8_t type,
                              uint16_t sector_size_hint, fn_disk_info_t *info)
{
    uint16_t uri_length;
    uint16_t reply_length = 0;
    uint8_t result;

    if (context == NULL || uri == NULL) return FN_ERR_INVALID;
    uri_length = (uint16_t)strlen(uri);
    if ((uint32_t)8 + uri_length > sizeof(context->codec_scratch)) {
        return FN_ERR_INVALID;
    }
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    context->codec_scratch[2] = readonly ? 1 : 0;
    context->codec_scratch[3] = type;
    put_u16le(context->codec_scratch + 4, sector_size_hint);
    put_u16le(context->codec_scratch + 6, uri_length);
    memcpy(context->codec_scratch + 8, uri, uri_length);
    result = context_disk_call(context, FN_DISK_CMD_MOUNT,
                               (uint16_t)(8 + uri_length), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 12 ||
        context->codec_scratch[0] != FN_DISK_PROTOCOL_VERSION) {
        return FN_ERR_IO;
    }
    if (info != NULL) {
        info->flags = context->codec_scratch[1];
        info->slot = context->codec_scratch[4];
        info->type = context->codec_scratch[5];
        info->sector_size = get_u16le(context->codec_scratch + 6);
        info->sector_count = get_u32le(context->codec_scratch + 8);
        info->last_error = 0;
    }
    return FN_OK;
}

uint8_t fn_disk_info_context(fn_disk_client_context_t *context, uint8_t slot,
                             fn_disk_info_t *info)
{
    uint16_t reply_length = 0;
    uint8_t result;
    if (context == NULL || info == NULL) return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    result = context_disk_call(context, FN_DISK_CMD_INFO, 2, &reply_length);
    if (result != FN_OK) return result;
    return parse_info(context->codec_scratch, reply_length, info);
}

uint8_t fn_disk_restore_boot_context(fn_disk_client_context_t *context,
                                     uint8_t slot, fn_disk_info_t *info)
{
    uint16_t reply_length = 0;
    uint8_t result;
    if (context == NULL || info == NULL) return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    result = context_disk_call(context, FN_DISK_CMD_RESTORE_BOOT, 2,
                               &reply_length);
    if (result != FN_OK) return result;
    return parse_info(context->codec_scratch, reply_length, info);
}

uint8_t fn_disk_inspect_context(fn_disk_client_context_t *context,
                                const char *uri, uint8_t type,
                                uint16_t sector_size_hint,
                                fn_disk_inspection_t *inspection)
{
    uint16_t uri_length;
    uint16_t reply_length = 0;
    uint16_t boot_length;
    uint8_t result;
    if (context == NULL || uri == NULL || inspection == NULL) return FN_ERR_INVALID;
    uri_length = (uint16_t)strlen(uri);
    if ((uint32_t)10 + uri_length > sizeof(context->codec_scratch)) return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = 0;
    context->codec_scratch[2] = type;
    put_u16le(context->codec_scratch + 3, sector_size_hint);
    put_u16le(context->codec_scratch + 5, FN_DISK_INSPECT_BOOT_BYTES);
    put_u16le(context->codec_scratch + 7, uri_length);
    memcpy(context->codec_scratch + 9, uri, uri_length);
    result = context_disk_call(context, FN_DISK_CMD_INSPECT,
                               (uint16_t)(9 + uri_length), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length < 10 || context->codec_scratch[0] != FN_DISK_PROTOCOL_VERSION) return FN_ERR_IO;
    boot_length = get_u16le(context->codec_scratch + 8);
    if (boot_length > FN_DISK_INSPECT_BOOT_BYTES || reply_length != (uint16_t)(10 + boot_length)) return FN_ERR_IO;
    memset(inspection, 0, sizeof(*inspection));
    inspection->media.flags = FN_DISK_FLAG_MOUNTED | FN_DISK_FLAG_READONLY;
    inspection->media.type = context->codec_scratch[1];
    inspection->media.sector_size = get_u16le(context->codec_scratch + 2);
    inspection->media.sector_count = get_u32le(context->codec_scratch + 4);
    inspection->boot_length = boot_length;
    memcpy(inspection->boot_bytes, context->codec_scratch + 10, boot_length);
    return FN_OK;
}

uint8_t fn_disk_read_sector_context(fn_disk_client_context_t *context,
                                    uint8_t slot, uint32_t lba, uint8_t *data,
                                    uint16_t data_capacity,
                                    uint16_t *data_length)
{
    uint16_t reply_length = 0;
    uint16_t payload_length;
    uint8_t result;
    if (context == NULL || data == NULL || data_capacity == 0 ||
        data_length == NULL) return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    put_u32le(context->codec_scratch + 2, lba);
    put_u16le(context->codec_scratch + 6, data_capacity);
    result = context_disk_call(context, FN_DISK_CMD_READ_SECTOR, 8,
                               &reply_length);
    if (result != FN_OK) return result;
    if (reply_length < 11 ||
        context->codec_scratch[0] != FN_DISK_PROTOCOL_VERSION ||
        context->codec_scratch[4] != slot ||
        get_u32le(context->codec_scratch + 5) != lba) return FN_ERR_IO;
    payload_length = get_u16le(context->codec_scratch + 9);
    if ((uint32_t)11 + payload_length != reply_length ||
        payload_length > data_capacity) return FN_ERR_IO;
    memcpy(data, context->codec_scratch + 11, payload_length);
    *data_length = payload_length;
    return FN_OK;
}

static uint8_t context_slot_command(fn_disk_client_context_t *context,
                                    uint8_t command, uint8_t slot)
{
    uint16_t reply_length = 0;
    uint8_t result;
    if (context == NULL) return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    result = context_disk_call(context, command, 2, &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 5 ||
        context->codec_scratch[0] != FN_DISK_PROTOCOL_VERSION ||
        context->codec_scratch[4] != slot) return FN_ERR_IO;
    return FN_OK;
}

uint8_t fn_disk_write_sector_context(fn_disk_client_context_t *context,
                                     uint8_t slot, uint32_t lba,
                                     const uint8_t *data,
                                     uint16_t data_length)
{
    uint16_t reply_length = 0;
    uint8_t result;
    if (context == NULL || data == NULL || data_length == 0 ||
        (uint32_t)8 + data_length > sizeof(context->codec_scratch))
        return FN_ERR_INVALID;
    context->codec_scratch[0] = FN_DISK_PROTOCOL_VERSION;
    context->codec_scratch[1] = slot;
    put_u32le(context->codec_scratch + 2, lba);
    put_u16le(context->codec_scratch + 6, data_length);
    memcpy(context->codec_scratch + 8, data, data_length);
    result = context_disk_call(context, FN_DISK_CMD_WRITE_SECTOR,
                               (uint16_t)(8 + data_length), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 11 ||
        context->codec_scratch[0] != FN_DISK_PROTOCOL_VERSION ||
        context->codec_scratch[4] != slot ||
        get_u32le(context->codec_scratch + 5) != lba ||
        get_u16le(context->codec_scratch + 9) != data_length) return FN_ERR_IO;
    return FN_OK;
}

uint8_t fn_disk_unmount_context(fn_disk_client_context_t *context, uint8_t slot)
{ return context_slot_command(context, FN_DISK_CMD_UNMOUNT, slot); }
uint8_t fn_disk_clear_changed_context(fn_disk_client_context_t *context,
                                      uint8_t slot)
{ return context_slot_command(context, FN_DISK_CMD_CLEAR_CHANGED, slot); }
uint8_t fn_disk_flush_context(fn_disk_client_context_t *context, uint8_t slot)
{ return context_slot_command(context, FN_DISK_CMD_FLUSH, slot); }
#endif

uint8_t fn_disk_mount(uint8_t slot, const char *uri, uint8_t readonly,
                      uint8_t type, uint16_t sector_size_hint,
                      fn_disk_info_t *info)
{
#if !defined(__CC65__)
    uint8_t disk_request[FN_MAX_PACKET_SIZE];
    uint8_t disk_reply[FN_MAX_PACKET_SIZE];
#endif
    uint16_t uri_length;
    uint16_t request_length;
    uint16_t reply_length = 0;
    uint8_t result;

    if (!uri) return FN_ERR_INVALID;
#if !defined(__CC65__) && !defined(__WATCOMC__)
    if (strlen(uri) > 0xFFFFu) return FN_ERR_INVALID;
#endif
    uri_length = (uint16_t)strlen(uri);
    if ((uint32_t)8 + uri_length > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE) {
        return FN_ERR_INVALID;
    }
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    disk_request[2] = readonly ? 1 : 0;
    disk_request[3] = type;
    put_u16le(disk_request + 4, sector_size_hint);
    put_u16le(disk_request + 6, uri_length);
    memcpy(disk_request + 8, uri, uri_length);
    request_length = (uint16_t)(8 + uri_length);

    result = disk_call(FN_DISK_CMD_MOUNT, disk_request, request_length,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 12 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION) {
        return FN_ERR_IO;
    }
    if (info) {
        info->flags = disk_reply[1];
        info->slot = disk_reply[4];
        info->type = disk_reply[5];
        info->sector_size = get_u16le(disk_reply + 6);
        info->sector_count = get_u32le(disk_reply + 8);
        info->last_error = 0;
    }
    return FN_OK;
}

uint8_t fn_disk_restore_boot(uint8_t slot, fn_disk_info_t *info)
{
#if !defined(__CC65__)
    uint8_t disk_request[2];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;

    if (!info) return FN_ERR_INVALID;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    result = disk_call(FN_DISK_CMD_RESTORE_BOOT, disk_request, 2,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    return parse_info(disk_reply, reply_length, info);
}

uint8_t fn_disk_unmount(uint8_t slot)
{
#if !defined(__CC65__)
    uint8_t disk_request[2];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    result = disk_call(FN_DISK_CMD_UNMOUNT, disk_request, 2,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 5 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION ||
        disk_reply[4] != slot) return FN_ERR_IO;
    return FN_OK;
}

uint8_t fn_disk_flush(uint8_t slot)
{
#if !defined(__CC65__)
    uint8_t disk_request[2];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    result = disk_call(FN_DISK_CMD_FLUSH, disk_request, 2,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 5 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION ||
        disk_reply[4] != slot) return FN_ERR_IO;
    return FN_OK;
}

uint8_t fn_disk_info(uint8_t slot, fn_disk_info_t *info)
{
#if !defined(__CC65__)
    uint8_t disk_request[2];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;

    if (!info) return FN_ERR_INVALID;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    result = disk_call(FN_DISK_CMD_INFO, disk_request, 2,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    return parse_info(disk_reply, reply_length, info);
}

uint8_t fn_disk_clear_changed(uint8_t slot)
{
#if !defined(__CC65__)
    uint8_t disk_request[2];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    result = disk_call(FN_DISK_CMD_CLEAR_CHANGED, disk_request, 2,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 5 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION ||
        disk_reply[4] != slot) return FN_ERR_IO;
    return FN_OK;
}

uint8_t fn_disk_read_sector(uint8_t slot, uint32_t lba,
                            uint8_t *data, uint16_t data_capacity,
                            uint16_t *data_length)
{
#if !defined(__CC65__)
    uint8_t disk_request[8];
    uint8_t disk_reply[FN_MAX_PACKET_SIZE];
#endif
    uint16_t reply_length = 0;
    uint16_t payload_length;
    uint8_t result;

    if (!data || !data_capacity || !data_length) return FN_ERR_INVALID;
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    put_u32le(disk_request + 2, lba);
    put_u16le(disk_request + 6, data_capacity);
    result = disk_call(FN_DISK_CMD_READ_SECTOR, disk_request, 8,
                       disk_reply, sizeof(disk_reply), &reply_length);
    if (result != FN_OK) return result;
    if (reply_length < 11 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION ||
        disk_reply[4] != slot || get_u32le(disk_reply + 5) != lba) return FN_ERR_IO;
    payload_length = get_u16le(disk_reply + 9);
    if ((uint32_t)11 + payload_length != reply_length ||
        payload_length > data_capacity) return FN_ERR_IO;
    memcpy(data, disk_reply + 11, payload_length);
    *data_length = payload_length;
    return FN_OK;
}

uint8_t fn_disk_write_sector(uint8_t slot, uint32_t lba,
                             const uint8_t *data, uint16_t data_length)
{
#if !defined(__CC65__)
    uint8_t disk_request[FN_MAX_PACKET_SIZE];
    uint8_t disk_reply[16];
#endif
    uint16_t reply_length = 0;
    uint8_t result;

    if (!data || !data_length || (uint32_t)8 + data_length > FN_MAX_PACKET_SIZE - FN_HEADER_SIZE) {
        return FN_ERR_INVALID;
    }
    disk_request[0] = FN_DISK_PROTOCOL_VERSION;
    disk_request[1] = slot;
    put_u32le(disk_request + 2, lba);
    put_u16le(disk_request + 6, data_length);
    memcpy(disk_request + 8, data, data_length);
    result = disk_call(FN_DISK_CMD_WRITE_SECTOR, disk_request,
                       (uint16_t)(8 + data_length), disk_reply,
                       sizeof(disk_reply),
                       &reply_length);
    if (result != FN_OK) return result;
    if (reply_length != 11 || disk_reply[0] != FN_DISK_PROTOCOL_VERSION ||
        disk_reply[4] != slot || get_u32le(disk_reply + 5) != lba ||
        get_u16le(disk_reply + 9) != data_length) return FN_ERR_IO;
    return FN_OK;
}
