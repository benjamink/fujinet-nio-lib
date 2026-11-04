#include <stdio.h>
#include <string.h>

#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

static uint8_t last_device;
static uint8_t last_command;
static uint8_t last_request[64];
static uint16_t last_request_len;
static uint8_t reply_status;
static uint16_t reply_len;
static uint8_t reply[64];

#ifdef TEST_BBC_RAW
uint8_t fn_bbc_device_call_raw(uint8_t device, uint8_t command, const uint8_t *payload,
                    uint16_t payload_length, uint8_t *out, uint16_t out_capacity,
                    uint8_t *status, uint16_t *length)
#else
uint8_t fn_raw_call(uint8_t device, uint8_t command, const void *payload,
                    uint16_t payload_length, void *out, uint16_t out_capacity,
                    fn_raw_response_t *response)
#endif
{
    last_device = device;
    last_command = command;
    last_request_len = payload_length;
    memcpy(last_request, payload, payload_length);
    if (reply_len > out_capacity) {
        return FN_ERR_INVALID;
    }
    memcpy(out, reply, reply_len);
#ifdef TEST_BBC_RAW
    *status = reply_status;
    *length = reply_len;
#else
    response->status = reply_status;
    response->payload_length = reply_len;
#endif
    return FN_OK;
}

#define CHECK(cond) do { if (!(cond)) { printf("failed: %s (line %d)\n", #cond, __LINE__); return 1; } } while (0)

int main(void)
{
    static const uint8_t expect_request[] = {
        1,                      /* version */
        3, 0, 'a', ':', '/',    /* uri */
        5, 0,                   /* startPage 5 */
        9,                      /* pageSize */
        0xE0, 0x01,             /* maxPayloadBytes 480 */
        FN_LIST_SORT | FN_LIST_DIRS_FIRST,
        31,                     /* maxNameBytes */
        5, 'm', 'i', 'n', 'e', 'r' /* optional filter */
    };
    uint8_t buf[128];
    fn_file_io_t io = { buf, sizeof(buf) };
    fn_list_pages_t out;
    fn_list_pages_req_t rq = { 5, 9, 480, FN_LIST_SORT | FN_LIST_DIRS_FIRST, 31, "miner" };
    fn_list_pages_req_t plain = { 0, 9, 480, 0, 0, 0 };
    uint8_t r;

    /* a reply with one page of one entry "x": 0 dir flags, size 7, mtime 9 */
    memset(reply, 0, sizeof(reply));
    reply[0] = 1; reply[1] = FN_LIST_PAGES_MORE;
    reply[4] = 0x2C; reply[5] = 0x01;           /* total 300 */
    reply[8] = 5;                               /* startPage 5 echo */
    reply[10] = 9; reply[11] = 1;               /* pageSize 9, 1 page */
    reply[12] = 1;                              /* entryCount */
    reply[13] = 11; reply[14] = 0;              /* dataLen = 2 + 1 + 4 + 4 */
    reply[15] = 0; reply[16] = 1; reply[17] = 'x';
    reply[18] = 7;
    reply[22] = 9;
    reply_len = 26;
    reply_status = 0;

    r = fn_file_list_pages(&io, "a:/", &rq, &out);
    CHECK(r == FN_OK);
    CHECK(last_device == FN_DEVICE_FILE && last_command == FN_CMD_FILE_LIST_PAGES);
    CHECK(last_request_len == sizeof(expect_request));
    CHECK(memcmp(last_request, expect_request, sizeof(expect_request)) == 0);
    CHECK(out.flags == FN_LIST_PAGES_MORE);
    CHECK(out.total == 300u);
    CHECK(out.start_page == 5 && out.page_size == 9 && out.page_count == 1);
    CHECK(out.pages == buf + 12 && out.pages_len == 14);
    CHECK(out.pages[0] == 1 && out.pages[3] == 0 && out.pages[5] == 'x');

    CHECK(fn_file_list_pages(&io, "a:/", &plain, &out) == FN_OK);
    CHECK(last_request_len == sizeof(expect_request) - 6);
    CHECK(last_request[last_request_len - 1] == 0);

    /* device errors map to library errors */
    reply_status = 6;
    CHECK(fn_file_list_pages(&io, "a:/", &plain, &out) == FN_ERR_TIMEOUT);
    reply_status = 5;
    CHECK(fn_file_list_pages(&io, "a:/", &plain, &out) == FN_ERR_IO);

    /* a short or foreign reply is an I/O error */
    reply_status = 0; reply_len = 6;
    CHECK(fn_file_list_pages(&io, "a:/", &plain, &out) == FN_ERR_IO);

    /* bad arguments are refused without a call */
    CHECK(fn_file_list_pages(&io, "", &plain, &out) == FN_ERR_INVALID);
    {
        fn_list_pages_req_t none = { 0, 0, 480, 0, 0, 0 };
        fn_file_io_t tiny = { buf, 8 };

        CHECK(fn_file_list_pages(&io, "a:/", &none, &out) == FN_ERR_INVALID);
        CHECK(fn_file_list_pages(&tiny, "a:/", &plain, &out) == FN_ERR_INVALID);
        tiny.capacity = 15; /* fixed request fits, filter does not */
        CHECK(fn_file_list_pages(&tiny, "a:/", &rq, &out) == FN_ERR_INVALID);
    }
    puts("list pages wire test passed");
    return 0;
}
