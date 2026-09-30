#include <stdio.h>
#include <string.h>

#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

static uint8_t last_device;
static uint8_t last_command;
static uint8_t last_payload[1024];
static uint16_t last_payload_length;
static uint8_t response_status;
static int response_disk_error = -1; /* failure payload {1, error} when >= 0 */
static uint16_t info_response_length = 13;

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

uint8_t fn_raw_call(uint8_t device, uint8_t command,
                    const void *payload, uint16_t payload_length,
                    void *reply, uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    uint8_t *out = (uint8_t *)reply;
    const uint8_t *in = (const uint8_t *)payload;
    uint32_t lba;
    uint16_t data_length;
    uint16_t i;

    last_device = device;
    last_command = command;
    last_payload_length = payload_length;
    memcpy(last_payload, payload, payload_length);
    response->status = response_status;
    response->payload_length = 0;
    if (response_status != 0) {
        if (response_disk_error >= 0 && reply_capacity >= 2) {
            out[0] = 1;
            out[1] = (uint8_t)response_disk_error;
            response->payload_length = 2;
        }
        return FN_OK;
    }
    if (device != FN_DEVICE_DISK || payload_length < 2) return FN_ERR_INVALID;

    switch (command) {
        case 0x01:
            if (reply_capacity < 12) return FN_ERR_INVALID;
            out[0] = 1; out[1] = 1; out[2] = 0; out[3] = 0; out[4] = in[1];
            out[5] = FN_DISK_TYPE_RAW; put_u16le(out + 6, 512);
            put_u32le(out + 8, 1760); response->payload_length = 12; break;
        case 0x02:
        case 0x06:
        case 0x0E:
            if (reply_capacity < 5) return FN_ERR_INVALID;
            out[0] = 1; out[1] = 0; out[2] = 0; out[3] = 0; out[4] = in[1];
            response->payload_length = 5; break;
        case 0x03:
            if (reply_capacity < 15 || payload_length != 8) return FN_ERR_INVALID;
            lba = (uint32_t)in[2] | ((uint32_t)in[3] << 8) |
                  ((uint32_t)in[4] << 16) | ((uint32_t)in[5] << 24);
            out[0] = 1; out[1] = 0; out[2] = 0; out[3] = 0; out[4] = in[1];
            put_u32le(out + 5, lba); put_u16le(out + 9, 4);
            for (i = 0; i < 4; ++i) out[11 + i] = (uint8_t)('A' + i);
            response->payload_length = 15; break;
        case 0x04:
            if (reply_capacity < 11 || payload_length < 9) return FN_ERR_INVALID;
            lba = (uint32_t)in[2] | ((uint32_t)in[3] << 8) |
                  ((uint32_t)in[4] << 16) | ((uint32_t)in[5] << 24);
            data_length = (uint16_t)in[6] | ((uint16_t)in[7] << 8);
            if ((uint32_t)8 + data_length != payload_length) return FN_ERR_INVALID;
            out[0] = 1; out[1] = 0; out[2] = 0; out[3] = 0; out[4] = in[1];
            put_u32le(out + 5, lba); put_u16le(out + 9, data_length);
            response->payload_length = 11; break;
        case 0x05:
            if (reply_capacity < 13) return FN_ERR_INVALID;
            out[0] = 1; out[1] = 0x0B; out[2] = 0; out[3] = 0; out[4] = in[1];
            out[5] = FN_DISK_TYPE_RAW; put_u16le(out + 6, 512);
            put_u32le(out + 8, 1760); out[12] = 5;
            response->payload_length = info_response_length; break;
        default:
            return FN_ERR_INVALID;
    }
    return FN_OK;
}

static int test_mount_info_read_write(void)
{
    fn_disk_info_t info;
    uint8_t data[8];
    uint8_t write_data[4] = {1, 2, 3, 4};
    uint16_t data_length = 0;

    response_status = 0;
    if (fn_disk_mount(1, "host:/images/test.adf", 1, FN_DISK_TYPE_AUTO, 0, &info) != FN_OK ||
        last_device != FN_DEVICE_DISK || last_command != 0x01 ||
        last_payload_length != 8 + strlen("host:/images/test.adf") ||
        last_payload[0] != 1 || last_payload[1] != 1 || last_payload[2] != 1 ||
        info.sector_size != 512 || info.sector_count != 1760) { puts("mount mismatch"); return 1; }
    if (fn_disk_info(1, &info) != FN_OK || info.flags != 0x0B) { puts("info mismatch"); return 1; }
    if (fn_disk_read_sector(1, 1759, data, sizeof(data), &data_length) != FN_OK ||
        data_length != 4 || memcmp(data, "ABCD", 4) != 0) { puts("read mismatch"); return 1; }
    if (fn_disk_write_sector(1, 1759, write_data, sizeof(write_data)) != FN_OK ||
        last_command != 0x04 || last_payload_length != 12) { puts("write mismatch"); return 1; }
    if (fn_disk_clear_changed(1) != FN_OK) { puts("clear mismatch"); return 1; }
    if (fn_disk_flush(1) != FN_OK || last_command != 0x0E ||
        last_payload_length != 2 || last_payload[0] != 1 ||
        last_payload[1] != 1) { puts("flush mismatch"); return 1; }
    if (fn_disk_unmount(1) != FN_OK) { puts("unmount mismatch"); return 1; }
    return 0;
}

static int test_mount_passes_explicit_type(void)
{
    fn_disk_info_t info;

    /* Types are the fujinet-nio disk::ImageType values, sent unchanged. */
    response_status = 0;
    if (fn_disk_mount(1, "host:/mac/system.image", 1, FN_DISK_TYPE_DC42, 0, &info) != FN_OK ||
        last_command != 0x01 || last_payload[3] != 5) { puts("dc42 type mismatch"); return 1; }
    return 0;
}

static int test_last_error_reports_why(void)
{
    fn_disk_info_t info;

    /* A failure NIO explains: the transport status, then the exact reason. */
    response_status = 2; /* InvalidRequest */
    response_disk_error = FN_DISK_ERR_GEOMETRY_REQUIRED;
    if (fn_disk_mount(1, "host:/apple2/dos33.dsk", 1, FN_DISK_TYPE_AUTO, 0, &info) != FN_ERR_INVALID ||
        fn_disk_last_error() != FN_DISK_ERR_GEOMETRY_REQUIRED) { puts("geometry-required mismatch"); return 1; }

    /* The client supplies the geometry and the mount succeeds. */
    response_status = 0;
    response_disk_error = -1;
    if (fn_disk_mount(1, "host:/apple2/dos33.dsk", 1, FN_DISK_TYPE_RAW, 256, &info) != FN_OK ||
        last_payload[4] != 0x00 || last_payload[5] != 0x01 ||
        fn_disk_last_error() != FN_DISK_ERR_NONE) { puts("hinted mount mismatch"); return 1; }

    /* A failure without a disk error (older NIO): not mistaken for success. */
    response_status = 4; /* NotReady */
    if (fn_disk_info(1, &info) != FN_ERR_NOT_READY ||
        fn_disk_last_error() != FN_DISK_ERR_UNREPORTED) { puts("unreported mismatch"); return 1; }

    response_status = 0;
    return 0;
}

static int test_validation_and_status(void)
{
    uint8_t data[4];
    uint16_t length;
    fn_disk_info_t info;

    response_status = 4;
    if (fn_disk_info(1, &info) != FN_ERR_NOT_READY) return 1;
    response_status = 0;
    if (fn_disk_mount(1, 0, 0, FN_DISK_TYPE_AUTO, 0, 0) != FN_ERR_INVALID ||
        fn_disk_read_sector(1, 0, data, 0, &length) != FN_ERR_INVALID ||
        fn_disk_write_sector(1, 0, 0, 1) != FN_ERR_INVALID) return 1;
    return 0;
}

static int test_info_response_with_optional_last_error(void)
{
    fn_disk_info_t info;

    response_status = 0;
    info_response_length = 13;
    if (fn_disk_info(1, &info) != FN_OK || info.last_error != 5) return 1;
    return 0;
}

static int test_info_response_without_optional_last_error(void)
{
    fn_disk_info_t info;

    response_status = 0;
    info_response_length = 12;
    if (fn_disk_info(1, &info) != FN_OK || info.last_error != 0) return 1;
    info_response_length = 13;
    return 0;
}

static int test_protocol_vectors(void)
{
    static const uint8_t mount_request[] = {
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x15, 0x00,
        'h', 'o', 's', 't', ':', '/', 'i', 'm', 'a', 'g', 'e', 's', '/',
        't', 'e', 's', 't', '.', 'a', 'd', 'f'
    };
    static const uint8_t read_request[] = {
        0x01, 0x01, 0xDF, 0x06, 0x00, 0x00, 0x08, 0x00
    };
    static const uint8_t write_request[] = {
        0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x01, 0x02, 0x03, 0x04
    };

    response_status = 0;
    if (fn_disk_mount(1, "host:/images/test.adf", 1, FN_DISK_TYPE_AUTO, 0, 0) != FN_OK ||
        last_payload_length != sizeof(mount_request) ||
        memcmp(last_payload, mount_request, sizeof(mount_request)) != 0) return 1;
    {
        uint8_t data[8];
        uint16_t length = 0;
        if (fn_disk_read_sector(1, 1759, data, 8, &length) != FN_OK ||
            last_payload_length != sizeof(read_request) ||
            memcmp(last_payload, read_request, sizeof(read_request)) != 0) return 1;
    }
    {
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        if (fn_disk_write_sector(1, 2, data, sizeof(data)) != FN_OK ||
            last_payload_length != sizeof(write_request) ||
            memcmp(last_payload, write_request, sizeof(write_request)) != 0) return 1;
    }
    return 0;
}

int main(void)
{
    if (test_mount_info_read_write() || test_mount_passes_explicit_type() ||
        test_last_error_reports_why() ||
        test_validation_and_status() ||
        test_info_response_with_optional_last_error() ||
        test_info_response_without_optional_last_error() ||
        test_protocol_vectors()) {
        puts("disk wire tests failed");
        return 1;
    }
    puts("disk wire tests passed");
    return 0;
}
