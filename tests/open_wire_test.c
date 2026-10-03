#include <stdio.h>
#include <string.h>

#include "fn_internal.h"
#include "fn_platform.h"
#include "fn_protocol.h"
#include "fujinet-nio.h"

/* Drives fn_open() and fn_open_translated() through a scripted fake transport:
 * fn_transport_exchange() below records the request bytes and plays back the
 * reply set by the test. */

#define TEST_URL "https://example.com/a.png"
#define TEST_HANDLE 0x1234
#define TEST_PROTO_FLAGS 0x05

static int failures;

static uint8_t sent[FN_MAX_PACKET_SIZE];
static uint16_t sent_len;
static unsigned exchange_calls;
static uint8_t transport_result;
static uint8_t reply[32];
static uint16_t reply_len;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

uint8_t fn_transport_exchange(void)
{
    ++exchange_calls;
    sent_len = _fn_transport_ctx.req_len;
    memcpy(sent, _fn_transport_ctx.request, sent_len);
    if (transport_result != FN_OK) {
        return transport_result;
    }
    memcpy(_fn_transport_ctx.response, reply, reply_len);
    _fn_transport_ctx.resp_len = reply_len;
    return FN_OK;
}

/* A successful Open reply: header, then version, flags, two reserved bytes,
 * handle (LE16) and protocol flags. */
static void script_open_reply(uint8_t resp_flags)
{
    uint16_t offset;

    offset = fn_build_header(reply, FN_DEVICE_NETWORK, FN_CMD_OPEN, FN_HEADER_SIZE + 7);
    reply[offset++] = FN_PROTOCOL_VERSION;
    reply[offset++] = resp_flags;
    reply[offset++] = 0;
    reply[offset++] = 0;
    reply[offset++] = (uint8_t)(TEST_HANDLE & 0xFF);
    reply[offset++] = (uint8_t)(TEST_HANDLE >> 8);
    reply[offset++] = TEST_PROTO_FLAGS;
    reply[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(reply, offset);
    reply_len = offset;
}

static void reset(void)
{
    memset(_fn_sessions, 0, sizeof _fn_sessions);
    _fn_initialized = 1;
    exchange_calls = 0;
    sent_len = 0;
    transport_result = FN_OK;
    script_open_reply(0);
}

/* The plain Open request, spelled out byte by byte. */
static uint16_t expect_open(uint8_t *out, uint8_t method, uint8_t wire_flags, const char *url)
{
    uint16_t offset;
    uint16_t url_len;
    uint16_t total;

    url_len = (uint16_t)strlen(url);
    total = (uint16_t)(FN_HEADER_SIZE + 1 + 1 + 1 + 2 + url_len + 2 + 4 + 2);
    offset = 0;
    out[offset++] = FN_DEVICE_NETWORK;
    out[offset++] = FN_CMD_OPEN;
    out[offset++] = (uint8_t)(total & 0xFF);
    out[offset++] = (uint8_t)(total >> 8);
    out[offset++] = 0;
    out[offset++] = 0;
    out[offset++] = FN_PROTOCOL_VERSION;
    out[offset++] = method;
    out[offset++] = wire_flags;
    out[offset++] = (uint8_t)(url_len & 0xFF);
    out[offset++] = (uint8_t)(url_len >> 8);
    memcpy(out + offset, url, url_len);
    offset = (uint16_t)(offset + url_len);
    memset(out + offset, 0, 8);
    offset = (uint16_t)(offset + 8);
    out[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(out, offset);
    return offset;
}

/* The Open request followed by the translation extension block. */
static uint16_t expect_open_ext(uint8_t *out, uint8_t method, uint8_t wire_flags, const char *url,
                                uint8_t ttype, uint8_t tflags, const char *selector)
{
    uint16_t offset;
    uint16_t sel_len;

    offset = expect_open(out, method, wire_flags, url);
    sel_len = (uint16_t)strlen(selector);
    out[offset++] = 1;
    out[offset++] = 0;
    out[offset++] = 0;
    out[offset++] = 0;
    out[offset++] = ttype;
    out[offset++] = tflags;
    out[offset++] = (uint8_t)(sel_len & 0xFF);
    out[offset++] = (uint8_t)(sel_len >> 8);
    memcpy(out + offset, selector, sel_len);
    offset = (uint16_t)(offset + sel_len);
    out[2] = (uint8_t)(offset & 0xFF);
    out[3] = (uint8_t)(offset >> 8);
    out[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(out, offset);
    return offset;
}

static void check_sent(const uint8_t *expected, uint16_t expected_len)
{
    CHECK(sent_len == expected_len);
    CHECK(memcmp(sent, expected, expected_len) == 0);
}

static void check_session_recorded(uint8_t needs_body)
{
    CHECK(_fn_sessions[0].active == 1);
    CHECK(_fn_sessions[0].handle == TEST_HANDLE);
    CHECK(_fn_sessions[0].proto_flags == TEST_PROTO_FLAGS);
    CHECK(_fn_sessions[0].needs_body == needs_body);
    CHECK(_fn_sessions[0].write_offset == 0);
    CHECK(_fn_sessions[0].read_offset == 0);
}

static void test_fn_open(void)
{
    static uint8_t expected[FN_MAX_PACKET_SIZE];
    uint16_t expected_len;
    fn_handle_t handle;

    reset();
    handle = FN_INVALID_HANDLE;
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_FOLLOW_REDIR) == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_FOLLOW_REDIR, TEST_URL);
    check_sent(expected, expected_len);
    CHECK(handle == TEST_HANDLE);
    check_session_recorded(0);

    /* A reply that asks for a body marks the session. */
    reset();
    script_open_reply(FN_OPEN_RESP_NEEDS_BODY);
    CHECK(fn_open(&handle, FN_METHOD_POST, TEST_URL, 0) == FN_OK);
    check_session_recorded(1);

    /* A transport failure is returned and nothing is recorded. */
    reset();
    transport_result = FN_ERR_TIMEOUT;
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, 0) == FN_ERR_TIMEOUT);
    CHECK(_fn_sessions[0].active == 0);
}

static void test_flag_mapping(void)
{
    static uint8_t expected[FN_MAX_PACKET_SIZE];
    uint16_t expected_len;
    fn_handle_t handle;

    reset();
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_FOLLOW_REDIR) == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_FOLLOW_REDIR, TEST_URL);
    check_sent(expected, expected_len);

    reset();
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_BODY_UNKNOWN) == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_BODY_UNKNOWN, TEST_URL);
    check_sent(expected, expected_len);

    reset();
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_ALLOW_EVICT) == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_ALLOW_EVICT, TEST_URL);
    check_sent(expected, expected_len);

    reset();
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_STREAM_NO_PROBE) == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_STREAM_NO_PROBE, TEST_URL);
    check_sent(expected, expected_len);

    /* fn_open_translated() maps the flags the same way. */
    reset();
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL,
                             FN_OPEN_FOLLOW_REDIR | FN_OPEN_ALLOW_EVICT,
                             FN_TRANSLATE_IMAGE, 0, "fmt=ilbm") == FN_OK);
    expected_len = expect_open_ext(expected, FN_METHOD_GET,
                                   FN_OPEN_FLAG_FOLLOW_REDIR | FN_OPEN_FLAG_ALLOW_EVICT,
                                   TEST_URL, FN_TRANSLATE_IMAGE, 0, "fmt=ilbm");
    check_sent(expected, expected_len);
}

static void test_validation(void)
{
    static char long_url[FN_MAX_URL_LEN + 2];
    fn_handle_t handle;

    memset(long_url, 'a', FN_MAX_URL_LEN + 1);
    long_url[FN_MAX_URL_LEN + 1] = '\0';

    reset();
    _fn_initialized = 0;
    CHECK(fn_open(&handle, FN_METHOD_GET, TEST_URL, 0) == FN_ERR_INVALID);
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_IMAGE, 0, "fmt=ilbm") == FN_ERR_INVALID);

    reset();
    CHECK(fn_open(NULL, FN_METHOD_GET, TEST_URL, 0) == FN_ERR_INVALID);
    CHECK(fn_open(&handle, FN_METHOD_GET, NULL, 0) == FN_ERR_INVALID);
    CHECK(fn_open_translated(NULL, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_IMAGE, 0, "fmt=ilbm") == FN_ERR_INVALID);
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, NULL, 0,
                             FN_TRANSLATE_IMAGE, 0, "fmt=ilbm") == FN_ERR_INVALID);

    CHECK(fn_open(&handle, FN_METHOD_GET, long_url, 0) == FN_ERR_URL_TOO_LONG);
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, long_url, 0,
                             FN_TRANSLATE_IMAGE, 0, "fmt=ilbm") == FN_ERR_URL_TOO_LONG);

    /* None of the rejected calls reached the transport. */
    CHECK(exchange_calls == 0);
}

static void test_translated(void)
{
    static uint8_t expected[FN_MAX_PACKET_SIZE];
    const char *selector = "fmt=ilbm,w=640,h=400,colors=16";
    uint16_t expected_len;
    fn_handle_t handle;

    reset();
    handle = FN_INVALID_HANDLE;
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_FOLLOW_REDIR,
                             FN_TRANSLATE_IMAGE, 0, selector) == FN_OK);
    expected_len = expect_open_ext(expected, FN_METHOD_GET, FN_OPEN_FLAG_FOLLOW_REDIR,
                                   TEST_URL, FN_TRANSLATE_IMAGE, 0, selector);
    check_sent(expected, expected_len);
    CHECK(handle == TEST_HANDLE);
    check_session_recorded(0);

    /* Translation flags and each translation type go on the wire as given. */
    reset();
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_JSON, 0x03, "$.a") == FN_OK);
    expected_len = expect_open_ext(expected, FN_METHOD_GET, 0, TEST_URL,
                                   1, 0x03, "$.a");
    check_sent(expected, expected_len);

    reset();
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_XML, 0, "//item") == FN_OK);
    expected_len = expect_open_ext(expected, FN_METHOD_GET, 0, TEST_URL,
                                   2, 0, "//item");
    check_sent(expected, expected_len);

    reset();
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_RSS, 0, "") == FN_OK);
    expected_len = expect_open_ext(expected, FN_METHOD_GET, 0, TEST_URL,
                                   3, 0, "");
    check_sent(expected, expected_len);

    /* A transport failure is returned and nothing is recorded. */
    reset();
    transport_result = FN_ERR_IO;
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, 0,
                             FN_TRANSLATE_IMAGE, 0, selector) == FN_ERR_IO);
    CHECK(_fn_sessions[0].active == 0);
}

static void test_none_is_plain_open(void)
{
    static uint8_t expected[FN_MAX_PACKET_SIZE];
    uint16_t expected_len;
    fn_handle_t handle;

    /* FN_TRANSLATE_NONE ignores the selector and flags: exactly fn_open()'s bytes. */
    reset();
    CHECK(fn_open_translated(&handle, FN_METHOD_GET, TEST_URL, FN_OPEN_FOLLOW_REDIR,
                             FN_TRANSLATE_NONE, 0x7F, "fmt=ilbm,w=640") == FN_OK);
    expected_len = expect_open(expected, FN_METHOD_GET, FN_OPEN_FLAG_FOLLOW_REDIR, TEST_URL);
    check_sent(expected, expected_len);
    CHECK(handle == TEST_HANDLE);
    check_session_recorded(0);
}

int main(void)
{
    test_fn_open();
    test_flag_mapping();
    test_validation();
    test_translated();
    test_none_is_plain_open();

    if (failures != 0) {
        printf("open wire tests: %d failures\n", failures);
        return 1;
    }
    printf("open wire tests passed\n");
    return 0;
}
