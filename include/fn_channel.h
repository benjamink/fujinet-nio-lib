/**
 * @file fn_channel.h
 * @brief Low-level byte channel interface for stream transports.
 *
 * Channel implementations own platform-specific I/O only. They do not build
 * FujiBus packets, calculate checksums, or encode/decode transport frames.
 */

#ifndef FN_CHANNEL_H
#define FN_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
#endif

uint8_t fn_channel_init(void);
uint8_t fn_channel_ready(void);
uint8_t fn_channel_write_byte(uint8_t value, uint16_t timeout_ms);
uint8_t fn_channel_read_byte(uint8_t *value, uint16_t timeout_ms);
void fn_channel_drain_rx(void);
void fn_channel_close(void);

#ifdef FN_STREAM_WAKE_IDLE_MS
/* Milliseconds from any fixed point; may wrap. Only targets that reclaim the
 * line after an idle spell need it (fn_transport_stream.c). */
uint32_t fn_channel_now_ms(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FN_CHANNEL_H */
