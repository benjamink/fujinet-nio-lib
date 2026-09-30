#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fn_raw.h"
#include "fujinet-nio.h"

typedef struct exchange_fixture {
    fn_disk_client_context_t *nested_context;
    uint8_t nested_slot;
    uint8_t invoke_nested;
    uint8_t request_preserved;
} exchange_fixture_t;

static uint8_t checksum(const uint8_t *data, uint16_t length);

static uint8_t inspect_exchange(void *opaque, const uint8_t *request,
                                uint16_t request_length, uint8_t *response,
                                uint16_t response_capacity, uint16_t *response_length)
{
    const char *uri = "mem:/candidate.adf";
    uint16_t length = 21;
    (void)opaque;
    if (request_length != 33 || response_capacity < length ||
        request[0] != 0xFC || request[1] != 0x0F || request[6] != FN_DISK_PROTOCOL_VERSION ||
        request[7] != 0 || request[8] != FN_DISK_TYPE_AUTO || request[9] != 0 ||
        request[10] != 0 || request[11] != 0 || request[12] != 2 ||
        request[13] != 18 || request[14] != 0 || memcmp(request + 15, uri, 18) != 0)
        return FN_ERR_IO;
    memset(response, 0, length);
    response[0] = 0xFC; response[1] = 0x0F; response[2] = (uint8_t)length;
    response[5] = 1; response[6] = 0;
    response[7] = FN_DISK_PROTOCOL_VERSION;
    response[8] = FN_DISK_TYPE_RAW;
    response[9] = 0; response[10] = 2;
    response[11] = 0xC0; response[12] = 0x0D;
    response[15] = 4;
    memcpy(response + 17, "DOS\0", 4);
    response[4] = checksum(response, length);
    *response_length = length;
    return FN_OK;
}

/* NIO refusing a mount it cannot size: status InvalidRequest, {1, error}. */
static uint8_t geometry_required_exchange(void *opaque, const uint8_t *request,
                                          uint16_t request_length, uint8_t *response,
                                          uint16_t response_capacity,
                                          uint16_t *response_length)
{
    uint16_t length = 9;
    (void)opaque; (void)request_length;
    if (response_capacity < length || request[1] != 0x01) return FN_ERR_IO;
    memset(response, 0, length);
    response[0] = 0xFC; response[1] = 0x01; response[2] = (uint8_t)length;
    response[5] = 1; response[6] = 2; /* one u8 status param: InvalidRequest */
    response[7] = FN_DISK_PROTOCOL_VERSION;
    response[8] = FN_DISK_ERR_GEOMETRY_REQUIRED;
    response[4] = checksum(response, length);
    *response_length = length;
    return FN_OK;
}

uint8_t fn_raw_call(uint8_t device, uint8_t command, const void *payload,
                    uint16_t payload_length, void *reply,
                    uint16_t reply_capacity, fn_raw_response_t *response)
{
    (void)device; (void)command; (void)payload; (void)payload_length;
    (void)reply; (void)reply_capacity; (void)response;
    return FN_ERR_UNSUPPORTED;
}

static uint8_t checksum(const uint8_t *data, uint16_t length)
{
    uint16_t sum = 0;
    uint16_t i;
    for (i = 0; i < length; ++i) {
        sum += data[i];
        sum = (sum >> 8) + (sum & 0xFFu);
    }
    return (uint8_t)sum;
}

static uint8_t exchange(void *opaque, const uint8_t *request,
                        uint16_t request_length, uint8_t *response,
                        uint16_t response_capacity, uint16_t *response_length)
{
    exchange_fixture_t *fixture = opaque;
    uint8_t saved[8];
    fn_disk_info_t nested_info;
    uint8_t slot;
    uint16_t length = 20;

    if (request_length != 8 || response_capacity < length ||
        request[0] != 0xFC || request[1] != 0x05) return FN_ERR_IO;
    memcpy(saved, request, sizeof(saved));
    slot = request[7];
    if (fixture->invoke_nested) {
        fixture->invoke_nested = 0;
        if (fn_disk_info_context(fixture->nested_context,
                                 fixture->nested_slot,
                                 &nested_info) != FN_OK ||
            nested_info.slot != fixture->nested_slot) return FN_ERR_IO;
    }
    fixture->request_preserved =
        memcmp(saved, request, sizeof(saved)) == 0 ? 1 : 0;

    memset(response, 0, length);
    response[0] = 0xFC;
    response[1] = 0x05;
    response[2] = (uint8_t)length;
    response[5] = 1;
    response[6] = 0;
    response[7] = FN_DISK_PROTOCOL_VERSION;
    response[8] = FN_DISK_FLAG_MOUNTED | FN_DISK_FLAG_READONLY;
    response[11] = slot;
    response[12] = FN_DISK_TYPE_RAW;
    response[13] = 0;
    response[14] = 2;
    response[15] = 0xE0;
    response[16] = 0x06;
    response[4] = checksum(response, length);
    *response_length = length;
    return FN_OK;
}

int main(void)
{
    fn_disk_client_context_t first;
    fn_disk_client_context_t second;
    exchange_fixture_t first_fixture = { &second, 2, 1, 0 };
    exchange_fixture_t second_fixture = { NULL, 0, 0, 0 };
    fn_disk_info_t info;
    fn_disk_inspection_t inspection;

    if (fn_disk_context_init(&first, exchange, &first_fixture) != FN_OK ||
        fn_disk_context_init(&second, exchange, &second_fixture) != FN_OK ||
        fn_disk_info_context(&first, 1, &info) != FN_OK || info.slot != 1 ||
        !first_fixture.request_preserved ||
        !second_fixture.request_preserved ||
        fn_disk_context_init(&first, inspect_exchange, NULL) != FN_OK ||
        fn_disk_inspect_context(&first, "mem:/candidate.adf", FN_DISK_TYPE_AUTO,
                                0, &inspection) != FN_OK ||
        inspection.media.type != FN_DISK_TYPE_RAW ||
        inspection.media.sector_size != 512 ||
        inspection.media.sector_count != 3520 ||
        inspection.boot_length != 4 || memcmp(inspection.boot_bytes, "DOS\0", 4) != 0) {
        puts("interleaved DiskDevice contexts crossed state");
        return 1;
    }
    /* Each context reports why its own last call failed. */
    if (fn_disk_last_error_context(&first) != FN_DISK_ERR_NONE ||
        fn_disk_context_init(&second, geometry_required_exchange, NULL) != FN_OK ||
        fn_disk_mount_context(&second, 1, "mem:/dos33.dsk", 1, FN_DISK_TYPE_AUTO, 0,
                              &info) != FN_ERR_INVALID ||
        fn_disk_last_error_context(&second) != FN_DISK_ERR_GEOMETRY_REQUIRED ||
        fn_disk_last_error_context(&first) != FN_DISK_ERR_NONE ||
        fn_disk_last_error_context(NULL) != FN_DISK_ERR_UNREPORTED) {
        puts("DiskDevice context last error mismatch");
        return 1;
    }
    puts("independent DiskDevice contexts remain isolated");
    return 0;
}
