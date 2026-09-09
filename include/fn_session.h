/**
 * @file fn_session.h
 * @brief Persistent FujiBus session over a byte-oriented channel.
 *
 * The session owns SLIP framing and one synchronous request/response exchange.
 * Platform code supplies only byte I/O callbacks, making the same contract
 * usable for RS-232, TCP/PTY, and future byte-stream channels.
 */

#ifndef FN_SESSION_H
#define FN_SESSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t max_packet_size;
    uint16_t max_payload_size;
    uint8_t packet_native;
    uint8_t max_outstanding;
} fn_session_capabilities_t;

typedef struct {
    uint8_t (*open)(void *context);
    void (*close)(void *context);
    uint8_t (*write_byte)(void *context, uint8_t value, uint16_t timeout_ms);
    uint8_t (*read_byte)(void *context, uint8_t *value, uint16_t timeout_ms);
    void (*flush)(void *context);
    /* Optional raw byte-stream fast path. The session still owns SLIP
     * framing; channels that implement this receive one complete encoded
     * frame and must preserve ordered stream semantics. */
    uint8_t (*write_bytes)(void *context, const uint8_t *data,
                           uint16_t length, uint16_t timeout_ms);
} fn_stream_channel_ops_t;

typedef struct {
    const fn_stream_channel_ops_t *ops;
    void *context;
    uint8_t *wire_buffer;
    uint16_t wire_capacity;
    uint16_t last_raw_length;
    uint16_t last_decoded_length;
    fn_session_capabilities_t capabilities;
    uint8_t opened;
    uint8_t busy;
} fn_stream_session_t;

/** Initialize a session object; does not open the physical channel. */
uint8_t fn_stream_session_init(fn_stream_session_t *session,
                               const fn_stream_channel_ops_t *ops,
                               void *context,
                               uint8_t *wire_buffer,
                               uint16_t wire_capacity);

/** Open/close the persistent underlying channel. */
uint8_t fn_stream_session_open(fn_stream_session_t *session);
void fn_stream_session_close(fn_stream_session_t *session);

/** Flush stale input before the next request. */
uint8_t fn_stream_session_flush(fn_stream_session_t *session);

/** Return the fixed capabilities of this SLIP byte-stream session. */
const fn_session_capabilities_t *fn_stream_session_capabilities(
    const fn_stream_session_t *session);

/**
 * Send one complete FujiBus packet and receive its matching response.
 *
 * The initial contract permits one outstanding request. Retry policy belongs
 * to the caller; this function never retries a request implicitly.
 */
uint8_t fn_stream_session_request(fn_stream_session_t *session,
                                   const uint8_t *request,
                                   uint16_t request_length,
                                   uint8_t *response,
                                   uint16_t response_capacity,
                                   uint16_t *response_length,
                                   uint16_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* FN_SESSION_H */
