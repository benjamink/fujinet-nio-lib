#include <stdio.h>
#include <string.h>

#include "fujinet-nio.h"
#include "fn_legacy_appkey.h"
#include "fn_protocol.h"
#include "fn_raw.h"

static unsigned call_count;
static uint8_t last_device;
static uint8_t last_command;
static uint8_t last_payload[512];
static uint16_t last_payload_len;
static uint8_t malformed_read;

static void put_u16le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void put_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint16_t get_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint8_t fn_raw_call(uint8_t device,
                    uint8_t command,
                    const void *payload,
                    uint16_t payload_length,
                    void *reply,
                    uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    const uint8_t *in = (const uint8_t *)payload;
    uint8_t *out = (uint8_t *)reply;

    ++call_count;
    last_device = device;
    last_command = command;
    last_payload_len = payload_length;
    if (payload_length > sizeof(last_payload)) {
        return FN_ERR_INVALID;
    }
    if (payload_length != 0) {
        memcpy(last_payload, payload, payload_length);
    }

    response->status = FN_OK;
    response->payload_length = 0;

    if (device != FN_DEVICE_FILE) {
        return FN_ERR_INVALID;
    }

    if (command == FN_CMD_FILE_MKDIR) {
        if (reply_capacity < 4) return FN_ERR_INVALID;
        out[0] = 1;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        response->payload_length = 4;
        return FN_OK;
    }

    if (command == FN_CMD_FILE_WRITE) {
        uint16_t uri_len = get_u16le(&in[1]);
        uint16_t data_len = get_u16le(&in[3 + uri_len + 4]);
        if (reply_capacity < 10) return FN_ERR_INVALID;
        out[0] = 1;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        put_u32le(&out[4], 0);
        put_u16le(&out[8], data_len);
        response->payload_length = 10;
        return FN_OK;
    }

    if (command == FN_CMD_FILE_READ) {
        static const uint8_t value[] = {'X', 'Y', 'Z'};
        if (reply_capacity < 13) return FN_ERR_INVALID;
        out[0] = 1;
        out[1] = 1;
        out[2] = 0;
        out[3] = 0;
        put_u32le(&out[4], 0);
        put_u16le(&out[8], sizeof(value));
        memcpy(&out[10], value, sizeof(value));
        response->payload_length = 13;
        if (malformed_read == 1) response->payload_length = 9;
        if (malformed_read == 2) out[4] = 1;
        if (malformed_read == 3) put_u16le(&out[8], 64);
        return FN_OK;
    }

    return FN_ERR_UNSUPPORTED;
}

static int expect_uri_payload(const char *uri)
{
    uint16_t uri_len = (uint16_t)strlen(uri);
    if (last_payload_len < (uint16_t)(3 + uri_len)) return 0;
    if (last_payload[0] != 1) return 0;
    if (get_u16le(&last_payload[1]) != uri_len) return 0;
    if (memcmp(&last_payload[3], uri, uri_len) != 0) return 0;
    return 1;
}

static int test_write_default_key(void)
{
    static uint8_t value[] = {'A', 'B', 'C'};
    call_count = 0;
    fuji_set_appkey_details(0xfe0c, 1, DEFAULT);

    if (!fuji_write_appkey(1, sizeof(value), value)) {
        puts("write failed");
        return 1;
    }
    if (call_count != 2) {
        puts("write did not mkdir then write");
        return 1;
    }
    if (last_device != FN_DEVICE_FILE || last_command != FN_CMD_FILE_WRITE) {
        puts("write used wrong device/command");
        return 1;
    }
    if (!expect_uri_payload("persist:///FujiNet/fe0c0101.key")) {
        puts("write URI mismatch");
        return 1;
    }
    if (memcmp(&last_payload[last_payload_len - 3], value, sizeof(value)) != 0) {
        puts("write value mismatch");
        return 1;
    }
    return 0;
}

static int test_read_default_key(void)
{
    uint8_t value[64];
    uint16_t count = 0;
    call_count = 0;
    fuji_set_appkey_details(0xfe0c, 1, DEFAULT);

    if (!fuji_read_appkey(1, &count, value)) {
        puts("read failed");
        return 1;
    }
    if (call_count != 1 ||
        last_device != FN_DEVICE_FILE ||
        last_command != FN_CMD_FILE_READ ||
        !expect_uri_payload("persist:///FujiNet/fe0c0101.key")) {
        puts("read request mismatch");
        return 1;
    }
    if (count != 3 || memcmp(value, "XYZ", 3) != 0) {
        puts("read value mismatch");
        return 1;
    }
    return 0;
}

static int test_buffered_read(void)
{
    uint8_t data[256], scratch[266];
    uint16_t count = 99;
    fuji_set_appkey_details(0x1234, 0xab, DEFAULT);
    call_count = 0;
    if (fuji_read_appkey_buffered(0xcd, &count, data, scratch, 73) || count != 0 || call_count != 0)
        return puts("short scratch accepted"), 1;
    if (!fuji_read_appkey_buffered(0xcd, &count, data, scratch, 74) ||
        count != 3 || memcmp(data, "XYZ", 3) || !expect_uri_payload("persist:///FujiNet/1234abcd.key"))
        return puts("buffered DEFAULT read failed"), 1;
    if (last_payload_len != 40 || memcmp(last_payload + 34, "\0\0\0\0\x40\0", 6))
        return puts("buffered DEFAULT read range mismatch"), 1;
    fuji_set_appkey_details(0x1234, 0xab, SIZE_256);
    count = 99; call_count = 0;
    if (fuji_read_appkey_buffered(0xcd, &count, data, scratch, 265) || count != 0 || call_count != 0)
        return puts("short SIZE_256 scratch accepted"), 1;
    if (!fuji_read_appkey_buffered(0xcd, &count, data, scratch, sizeof(scratch)) || count != 3)
        return puts("buffered SIZE_256 read failed"), 1;
    if (last_payload_len != 40 || memcmp(last_payload + 34, "\0\0\0\0\0\x01", 6))
        return puts("buffered SIZE_256 read range mismatch"), 1;
    fuji_set_appkey_details(0x1234, 0xab, DEFAULT);
    for (malformed_read = 1; malformed_read <= 3; ++malformed_read) {
        memset(data, 0xa5, sizeof(data));
        count = 99;
        if (fuji_read_appkey_buffered(0xcd, &count, data, scratch, 74) ||
            count != 0 || data[0] != 0xa5)
            return puts("malformed buffered read accepted"), 1;
    }
    malformed_read = 0;
    return 0;
}

static int test_size_limits(void)
{
    uint8_t value[65];
    memset(value, 0x42, sizeof(value));

    call_count = 0;
    fuji_set_appkey_details(0xfe0c, 1, DEFAULT);
    if (fuji_write_appkey(1, sizeof(value), value)) {
        puts("default accepted oversized key");
        return 1;
    }
    if (call_count != 0) {
        puts("oversized key performed I/O");
        return 1;
    }

    fuji_set_appkey_details(0xfe0c, 1, SIZE_256);
    if (!fuji_write_appkey(1, sizeof(value), value)) {
        puts("SIZE_256 rejected 65-byte key");
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_write_default_key()) return 1;
    if (test_read_default_key()) return 1;
    if (test_buffered_read()) return 1;
    if (test_size_limits()) return 1;
    puts("legacy appkey wire tests passed");
    return 0;
}
