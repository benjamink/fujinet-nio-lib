/**
 * @file fn_transport_stream.c
 * @brief Adapter from the library transport state to the shared stream session.
 *
 * Platform channel implementations provide byte I/O. The shared session owns
 * SLIP framing, flushing, timeouts, and the one-outstanding-request rule.
 */

#include "fujinet-nio.h"
#include "fn_channel.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "fn_session.h"
#include "fn_protocol.h"

#ifndef FN_TRANSPORT_WIRE_BUF_SIZE
#define FN_TRANSPORT_WIRE_BUF_SIZE ((FN_MAX_PACKET_SIZE * 2) + 2)
#endif

static uint8_t _stream_wire_buf[FN_TRANSPORT_WIRE_BUF_SIZE];
static fn_stream_session_t _stream_session;
static uint8_t _stream_session_initialized;

#ifdef FN_STREAM_WAKE_IDLE_MS
/*
 * Some lines are lent to another protocol while the bus is idle: a Palm cradle
 * on FujiNet NIO is lent to HotSync for 1.5 s windows, and a request sent then
 * is dropped unanswered. Any FujiBus request keeps the line for the bus for a
 * minute, so before the first request after an idle spell the transport
 * reclaims it with FujiDevice GetInfo, which is harmless to repeat: a short
 * wait, several times, until something answers. The real request is then sent
 * once, as on any other line.
 */
#ifndef FN_STREAM_WAKE_WAIT_MS
#define FN_STREAM_WAKE_WAIT_MS 700
#endif
#ifndef FN_STREAM_WAKE_ATTEMPTS
#define FN_STREAM_WAKE_ATTEMPTS 8
#endif

/* FujiDevice GetInfo, version 1. Its reply is a few dozen bytes. */
#define FN_WAKE_REQUEST_LENGTH (FN_HEADER_SIZE + 1)
#define FN_WAKE_REPLY_MAX      128

static uint8_t _stream_used;
static uint32_t _stream_last_reply_ms;

static void stream_wake(void)
{
    uint8_t request[FN_WAKE_REQUEST_LENGTH];
    uint8_t reply[FN_WAKE_REPLY_MAX];
    uint16_t reply_length;
    uint8_t attempt;

    if (_stream_used &&
        (uint32_t)(fn_channel_now_ms() - _stream_last_reply_ms) < FN_STREAM_WAKE_IDLE_MS) {
        return;
    }
    fn_build_header(request, FN_DEVICE_FUJI, FN_FUJI_CMD_GET_INFO,
                    FN_WAKE_REQUEST_LENGTH);
    request[FN_HEADER_SIZE] = FN_FUJI_PROTOCOL_VERSION;
    request[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(request, FN_WAKE_REQUEST_LENGTH);
    /* Any reply will do, even one refusing the command: the line is back. */
    for (attempt = 0; attempt < FN_STREAM_WAKE_ATTEMPTS; ++attempt) {
        if (fn_stream_session_request(&_stream_session, request,
                                      FN_WAKE_REQUEST_LENGTH, reply,
                                      sizeof(reply), &reply_length,
                                      FN_STREAM_WAKE_WAIT_MS) == FN_OK) {
            _stream_used = 1;
            _stream_last_reply_ms = fn_channel_now_ms();
            return;
        }
    }
}
#endif

static uint8_t channel_open(void *context)
{
    (void)context;
    return fn_channel_init();
}

static void channel_close(void *context)
{
    (void)context;
    fn_channel_close();
}

static uint8_t channel_write_byte(void *context, uint8_t value,
                                  uint16_t timeout_ms)
{
    (void)context;
    return fn_channel_write_byte(value, timeout_ms);
}

static uint8_t channel_read_byte(void *context, uint8_t *value,
                                 uint16_t timeout_ms)
{
    (void)context;
    return fn_channel_read_byte(value, timeout_ms);
}

static void channel_flush(void *context)
{
    (void)context;
    fn_channel_drain_rx();
}

static const fn_stream_channel_ops_t _stream_channel_ops = {
    channel_open,
    channel_close,
    channel_write_byte,
    channel_read_byte,
    channel_flush,
    NULL
};

uint8_t fn_transport_init(void)
{
    uint8_t result;

    if (!_stream_session_initialized) {
        result = fn_stream_session_init(&_stream_session, &_stream_channel_ops,
                                        0, _stream_wire_buf,
                                        sizeof(_stream_wire_buf));
        if (result != FN_OK) return result;
        _stream_session_initialized = 1;
    }
    return fn_stream_session_open(&_stream_session);
}

uint8_t fn_transport_ready(void)
{
    return _stream_session.opened;
}

uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request,
                                         _fn_transport_ctx.req_len,
                                         _fn_transport_ctx.response,
                                         _fn_transport_ctx.resp_max,
                                         &_fn_transport_ctx.resp_len);
}

uint8_t fn_transport_exchange_buffers(const uint8_t *request,
                                      uint16_t request_length,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length)
{
#ifdef FN_STREAM_WAKE_IDLE_MS
    uint8_t result;

    stream_wake();
    result = fn_stream_session_request(&_stream_session, request,
                                       request_length, response,
                                       response_capacity, response_length,
                                       FN_TRANSPORT_TIMEOUT);
    if (result == FN_OK) {
        _stream_used = 1;
        _stream_last_reply_ms = fn_channel_now_ms();
    }
    return result;
#else
    return fn_stream_session_request(&_stream_session, request,
                                     request_length, response,
                                     response_capacity, response_length,
                                     FN_TRANSPORT_TIMEOUT);
#endif
}

void fn_transport_close(void)
{
    fn_stream_session_close(&_stream_session);
}
