/* The stream transport's reclaim after an idle spell (FN_STREAM_WAKE_IDLE_MS),
 * against a fake line that drops requests the way a Palm cradle lent to
 * HotSync does. */
#include <stdio.h>
#include <string.h>

#include "fn_channel.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "fn_protocol.h"
#include "fn_slip.h"

fn_transport_ctx_t _fn_transport_ctx;

static uint8_t _frame[64];
static uint16_t _frame_length;
static uint8_t _reply[64];
static uint16_t _reply_length;
static uint16_t _reply_offset;
static uint16_t _frames;
static uint16_t _drop;          /* frames to leave unanswered */
static uint8_t _last_device;
static uint8_t _last_command;
static uint32_t _now;

uint8_t fn_channel_init(void) { return FN_OK; }
uint8_t fn_channel_ready(void) { return 1; }
void fn_channel_close(void) {}
uint32_t fn_channel_now_ms(void) { return _now; }

void fn_channel_drain_rx(void)
{
    _reply_length = 0;
    _reply_offset = 0;
}

/* Answers each request frame with a bare header echoing device and command. */
static void frame_done(void)
{
    uint8_t packet[64];
    uint8_t reply[FN_HEADER_SIZE];
    uint16_t length = fn_slip_decode(_frame, _frame_length, packet);

    _frame_length = 0;
    if (length < FN_HEADER_SIZE)
        return;
    _frames++;
    _last_device = packet[0];
    _last_command = packet[1];
    if (_drop > 0) {
        _drop--;
        return;
    }
    fn_build_header(reply, packet[0], packet[1], FN_HEADER_SIZE);
    reply[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(reply, FN_HEADER_SIZE);
    _reply_length = fn_slip_encode(reply, FN_HEADER_SIZE, _reply);
    _reply_offset = 0;
}

uint8_t fn_channel_write_byte(uint8_t value, uint16_t timeout_ms)
{
    (void)timeout_ms;
    if (_frame_length >= sizeof(_frame))
        return FN_ERR_IO;
    _frame[_frame_length++] = value;
    if (value == SLIP_END && _frame_length > 2)
        frame_done();
    else if (value == SLIP_END)
        _frame_length = 1;
    return FN_OK;
}

uint8_t fn_channel_read_byte(uint8_t *value, uint16_t timeout_ms)
{
    (void)timeout_ms;
    if (_reply_offset >= _reply_length)
        return 0;
    *value = _reply[_reply_offset++];
    return 1;
}

static uint8_t exchange(void)
{
    uint8_t request[FN_HEADER_SIZE];
    uint8_t response[64];
    uint16_t response_length = 0;

    fn_build_header(request, FN_DEVICE_NETWORK, 0x02, FN_HEADER_SIZE);
    request[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(request, FN_HEADER_SIZE);
    return fn_transport_exchange_buffers(request, sizeof(request), response,
                                         sizeof(response), &response_length);
}

static int expect(int ok, const char *what)
{
    if (!ok)
        printf("FAIL: %s\n", what);
    return !ok;
}

int main(void)
{
    int failed = 0;

    if (fn_transport_init() != FN_OK) {
        puts("FAIL: transport init");
        return 1;
    }

    /* First request: the line is lent, so three reclaims go unanswered. */
    _drop = 3;
    failed |= expect(exchange() == FN_OK, "request after a lent line");
    failed |= expect(_frames == 5, "three lost reclaims, one answered, then the request");
    failed |= expect(_last_device == FN_DEVICE_NETWORK, "the request goes last");

    /* Soon after: the line is the bus's, no reclaim. */
    _frames = 0;
    _now += FN_STREAM_WAKE_IDLE_MS - 1;
    failed |= expect(exchange() == FN_OK && _frames == 1, "no reclaim while recent");

    /* After an idle spell: one reclaim, then the request. */
    _frames = 0;
    _now += FN_STREAM_WAKE_IDLE_MS;
    failed |= expect(exchange() == FN_OK && _frames == 2, "one reclaim after idle");

    /* A dead line: every reclaim, then the request once, which times out. */
    _frames = 0;
    _now += FN_STREAM_WAKE_IDLE_MS;
    _drop = 100;
    failed |= expect(exchange() == FN_ERR_TIMEOUT, "dead line times out");
    failed |= expect(_frames == FN_STREAM_WAKE_ATTEMPTS + 1, "all reclaims, then the request once");

    /* The line answers again: one reclaim, then the request. */
    _drop = 0;
    _frames = 0;
    failed |= expect(exchange() == FN_OK && _frames == 2, "line back after a dead spell");

    puts(failed ? "stream wake tests failed" : "stream wake tests passed");
    return failed;
}
