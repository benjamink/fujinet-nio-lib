#include <cbm.h>

#include "fn_internal.h"
#include "fn_platform.h"
#include "fujinet-nio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(name, expression) do {                                      \
    if (!(expression)) {                                                  \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__);          \
        ++failures;                                                       \
    }                                                                     \
} while (0)

uint8_t g_c64_st;

/* The KERNAL calls in order, as text: "L30 S60 Daa Dbb U T30 K60 A A A X". */
static char g_log[8192];
static size_t g_log_len;

static void log_call(const char *fmt, unsigned value)
{
    g_log_len += (size_t)snprintf(g_log + g_log_len, sizeof(g_log) - g_log_len, fmt, value);
}

/* The device's side. */
static uint8_t g_absent;           /* not on the bus */
static uint8_t g_reply[1024];
static size_t g_reply_len;
static size_t g_reply_pos;
static uint8_t g_eoi_after_last;   /* flag EOI with the last byte, as a device does */
static uint8_t g_received[1024];
static size_t g_received_len;

void cbm_k_listen(uint8_t device)
{
    log_call("L%u ", device);
    if (g_absent) g_c64_st |= 0x80;
}

void cbm_k_second(uint8_t secondary)
{
    log_call("S%02x ", secondary);
}

void cbm_k_ciout(uint8_t value)
{
    log_call("D%02x ", value);
    g_received[g_received_len++] = value;
}

void cbm_k_unlsn(void)
{
    log_call("U ", 0);
}

void cbm_k_talk(uint8_t device)
{
    log_call("T%u ", device);
    if (g_absent) g_c64_st |= 0x80;
}

void cbm_k_tksa(uint8_t secondary)
{
    log_call("K%02x ", secondary);
}

uint8_t cbm_k_acptr(void)
{
    uint8_t value;

    log_call("A ", 0);
    if (g_reply_pos >= g_reply_len) {
        g_c64_st |= 0x42; /* no byte: read timeout and EOI */
        return 0;
    }
    value = g_reply[g_reply_pos++];
    if (g_reply_pos == g_reply_len && g_eoi_after_last) g_c64_st |= 0x40;
    return value;
}

void cbm_k_untlk(void)
{
    log_call("X ", 0);
}

static uint8_t g_response[64];

static void reset(void)
{
    g_c64_st = 0;
    g_log[0] = 0;
    g_log_len = 0;
    g_absent = 0;
    g_reply_len = 0;
    g_reply_pos = 0;
    g_eoi_after_last = 1;
    g_received_len = 0;
    memset(g_response, 0xEE, sizeof(g_response));
    _fn_transport_ctx.request = NULL;
    _fn_transport_ctx.req_len = 0;
    _fn_transport_ctx.response = g_response;
    _fn_transport_ctx.resp_max = sizeof(g_response);
    _fn_transport_ctx.resp_len = 0xFFFF;
}

/* A device reply: a 6-byte header with this total length, then a filler. */
static void set_reply(uint16_t length, size_t bytes_sent)
{
    size_t i;

    g_reply_len = bytes_sent;
    for (i = 0; i < bytes_sent; ++i) g_reply[i] = (uint8_t)(0x40 + i);
    g_reply[0] = 0xFD;
    g_reply[1] = 0x01;
    g_reply[2] = (uint8_t)length;
    g_reply[3] = (uint8_t)(length >> 8);
}

static void test_init_addresses_the_channel_and_sends_nothing(void)
{
    reset();
    CHECK("init ok", fn_transport_init() == FN_OK);
    CHECK("ready", fn_transport_ready() == 1);
    CHECK("init bus calls", strcmp(g_log, "L30 S60 U ") == 0);
    CHECK("nothing sent", g_received_len == 0);
    fn_transport_close();
    CHECK("closed", fn_transport_ready() == 0);
}

static void test_init_reports_an_absent_device(void)
{
    reset();
    g_absent = 1;
    CHECK("init fails", fn_transport_init() == FN_ERR_TRANSPORT);
    CHECK("not ready", fn_transport_ready() == 0);
}

static void test_init_clears_a_stale_status(void)
{
    reset();
    g_c64_st = 0x80; /* a failed drive access earlier */
    CHECK("init ok", fn_transport_init() == FN_OK);
}

static void test_exchange_sends_the_packet_then_reads_to_its_length(void)
{
    static const uint8_t request[] = { 0xFD, 0x01, 0x07, 0x00, 0x11, 0x00, 0x42 };

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(8, 8);

    CHECK("exchange ok", fn_transport_exchange() == FN_OK);
    CHECK("response length", _fn_transport_ctx.resp_len == 8);
    CHECK("response bytes", memcmp(g_response, g_reply, 8) == 0);
    CHECK("stops at the length", g_response[8] == 0xEE);
    CHECK("request bytes", g_received_len == sizeof(request) &&
          memcmp(g_received, request, sizeof(request)) == 0);
    CHECK("bus order", strcmp(g_log,
        "L30 S60 Dfd D01 D07 D00 D11 D00 D42 U T30 K60 A A A A A A A A X ") == 0);
}

static void test_exchange_stops_at_the_length_without_eoi(void)
{
    static const uint8_t request[] = { 0xFD };

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(6, 6);
    g_eoi_after_last = 0;

    CHECK("exchange ok", fn_transport_exchange() == FN_OK);
    CHECK("response length", _fn_transport_ctx.resp_len == 6);
}

static void test_exchange_clears_a_stale_status(void)
{
    static const uint8_t request[] = { 0xFD };

    reset();
    g_c64_st = 0x42;
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(6, 6);
    CHECK("stale status ignored", fn_transport_exchange() == FN_OK);
}

static void test_exchange_reports_an_absent_device_without_sending(void)
{
    static const uint8_t request[] = { 0xFD, 0x01 };

    reset();
    g_absent = 1;
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    CHECK("exchange fails", fn_transport_exchange() == FN_ERR_TRANSPORT);
    CHECK("nothing sent", g_received_len == 0);
    CHECK("no talk", strchr(g_log, 'T') == NULL);
}

static void test_exchange_reports_a_device_with_nothing_to_send(void)
{
    static const uint8_t request[] = { 0xFD };

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    g_reply_len = 0;

    CHECK("exchange fails", fn_transport_exchange() == FN_ERR_TRANSPORT);
    CHECK("untalk still sent", strstr(g_log, "X ") != NULL);
    CHECK("no length", _fn_transport_ctx.resp_len == 0);
}

static void test_exchange_reports_a_reply_cut_short(void)
{
    static const uint8_t request[] = { 0xFD };

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(20, 10); /* the header says 20; EOI comes with the tenth byte */

    CHECK("exchange fails", fn_transport_exchange() == FN_ERR_TRANSPORT);
    CHECK("untalk still sent", strstr(g_log, "X ") != NULL);
}

static void test_exchange_refuses_a_length_that_does_not_fit(void)
{
    static const uint8_t request[] = { 0xFD };

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(65, 65); /* one more than the caller's 64-byte buffer */
    CHECK("too long", fn_transport_exchange() == FN_ERR_TRANSPORT);
    CHECK("buffer untouched past the header", g_response[4] == 0xEE);

    reset();
    _fn_transport_ctx.request = request;
    _fn_transport_ctx.req_len = sizeof(request);
    set_reply(5, 5); /* shorter than a header */
    CHECK("too short", fn_transport_exchange() == FN_ERR_TRANSPORT);
}

int main(void)
{
    test_init_addresses_the_channel_and_sends_nothing();
    test_init_reports_an_absent_device();
    test_init_clears_a_stale_status();
    test_exchange_sends_the_packet_then_reads_to_its_length();
    test_exchange_stops_at_the_length_without_eoi();
    test_exchange_clears_a_stale_status();
    test_exchange_reports_an_absent_device_without_sending();
    test_exchange_reports_a_device_with_nothing_to_send();
    test_exchange_reports_a_reply_cut_short();
    test_exchange_refuses_a_length_that_does_not_fit();

    if (failures) {
        fprintf(stderr, "c64 transport host test: %u failure(s)\n", failures);
        return 1;
    }
    printf("c64 transport host test: all checks passed\n");
    return 0;
}
