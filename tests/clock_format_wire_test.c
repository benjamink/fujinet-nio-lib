#include <stdio.h>
#include <string.h>
#include "fn_internal.h"
static uint8_t response[128], expected_request[128];
static uint16_t response_length, expected_length;
static uint8_t wire_status;
#define CHECK(x) do { if (!(x)) { printf("clock format failure at %d\n", __LINE__); return 1; } } while (0)
uint8_t fn_clock_exchange(uint8_t command, const uint8_t *payload, uint16_t length,
 uint8_t *status, uint16_t *offset, uint16_t *count)
{
    if (command != FN_CMD_CLOCK_GET_FORMAT || length != expected_length ||
        memcmp(payload, expected_request, length)) return FN_ERR_INTERNAL;
    *status = wire_status; *offset = 0; *count = response_length;
    return FN_OK;
}
uint8_t fn_clock_response_byte(uint16_t offset) { return response[offset]; }
int main(void)
{
    uint8_t data[64], i, format;
    uint16_t size;
    for (format = 0; format <= 5; ++format) {
        memset(response, 'x', sizeof(response)); response[0] = 1; response[1] = format;
        memset(data, 0xa5, sizeof(data));
        size = fn_clock_format_capacity(format);
        response_length = (format == 3 || format == 4) ? 26 : (uint16_t)(size + 2);
        expected_request[0] = 1; expected_request[1] = format; expected_length = 2;
        CHECK(fn_clock_get_format(data, (FnTimeFormat)format) == FN_OK);
        if (format == 3 || format == 4) {
            CHECK(data[24] == 0 && data[25] == 0xa5);
        } else {
            for (i = 0; i < size; ++i) CHECK(data[i] == 'x');
            CHECK(data[size] == 0xa5);
        }
        memset(data, 0xa5, sizeof(data)); response[1] = (uint8_t)((format + 1) % 6);
        CHECK(fn_clock_get_format(data, (FnTimeFormat)format) == FN_ERR_IO);
        CHECK(data[0] == 0xa5);
        response[1] = format; response_length = 80;
        CHECK(fn_clock_get_format(data, (FnTimeFormat)format) == FN_ERR_IO);
        CHECK(data[0] == 0xa5 && data[63] == 0xa5);
    }
    response[0] = 2; response[1] = 0; response_length = 9;
    expected_request[1] = 0;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_SIMPLE) == FN_ERR_UNSUPPORTED);
    response[0] = 1; response_length = 8;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_SIMPLE) == FN_ERR_IO);
    /* cc65 stores this enum in one byte: casting 256 becomes valid format 0
     * before entering the API. Wider-enum targets must reject it. */
#ifndef __CC65__
    CHECK(fn_clock_get_format(data, (FnTimeFormat)256) == FN_ERR_INVALID);
#endif
    CHECK(fn_clock_get_tz(data, 0, FN_TIME_FORMAT_SIMPLE) == FN_ERR_INVALID);
    response_length = 9; expected_request[2] = 3;
    memcpy(expected_request + 3, "UTC", 3); expected_length = 6;
    CHECK(fn_clock_get_tz(data, "UTC", FN_TIME_FORMAT_SIMPLE) == FN_OK);
    wire_status = FN_ERR_NOT_READY;
    CHECK(fn_clock_get_tz(data, "UTC", FN_TIME_FORMAT_SIMPLE) == FN_ERR_NOT_READY);
    wire_status = FN_OK;
    expected_length = 2; expected_request[1] = FN_TIME_FORMAT_UTC_ISO;
    response[0] = 1; response[1] = FN_TIME_FORMAT_UTC_ISO;
    /* Both existing string encodings terminate within the caller's bound. */
    memset(response + 2, 's', 24); response[26] = 0; response_length = 27;
    memset(data, 0xa5, sizeof(data));
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_UTC_ISO) == FN_OK);
    CHECK(data[24] == 0 && data[25] == 0xa5);
    memset(data, 0xa5, sizeof(data)); response[5] = 0;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_UTC_ISO) == FN_ERR_IO);
    CHECK(data[0] == 0xa5 && data[63] == 0xa5);
    response[5] = 's'; response_length = 2;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_UTC_ISO) == FN_ERR_IO);
    size = fn_clock_format_capacity(FN_TIME_FORMAT_UTC_ISO);
    memset(response + 2, 's', size - 1); response_length = size + 1;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_UTC_ISO) == FN_OK);
    CHECK(data[size - 1] == 0 && data[size] == 0xa5);
    response[size + 1] = 's'; response_length++;
    CHECK(fn_clock_get_format(data, FN_TIME_FORMAT_UTC_ISO) == FN_ERR_IO);
    CHECK(fn_clock_get_format(data, (FnTimeFormat)-1) == FN_ERR_INVALID);
    CHECK(fn_clock_get_format(0, FN_TIME_FORMAT_SIMPLE) == FN_ERR_INVALID);
    puts("clock format wire tests passed");
    return 0;
}
