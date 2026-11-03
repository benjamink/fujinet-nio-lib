#ifndef FN_STREAM_H
#define FN_STREAM_H
#include <stdint.h>
/* Atari standard-speed stream API v1. Requires matching firmware.
 * One foreground caller, no calls from interrupts. Context and both buffers are
 * caller-owned and must survive retries. All registers/flags may change; A/X
 * carries the context pointer and A/X returns uint8 status (X=0). No C stack args.
 * Initialize once; then set length/data and call an operation. Open accepts a
 * lowercase tcp:// URL, length <=120, without NUL. Read length is capacity.
 * After write, actual is accepted count (may be partial); do not resend that
 * prefix. Status 3=busy, 4=not ready: call again later (new operation sequence).
 * Status 0x80=carrier failure: context retains sequence, repeat EXACT operation.
 * Status 0x81=invalid local input. No automatic reconnect or HSIO setting change.
 */
#define FN_STREAM_MAX 120
#define FN_STREAM_IO_ERROR 0x80
#define FN_STREAM_INVALID 0x81
typedef struct fn_stream_io {
    uint8_t version, operation;
    uint8_t token[4], sequence[4];
    uint8_t length, data[FN_STREAM_MAX];
    uint8_t status, flags, actual;
    uint8_t reply_token[4], reply_sequence[4], received[FN_STREAM_MAX];
} fn_stream_io_t;
#ifdef __ATARI__
uint8_t __fastcall__ fn_stream_init(fn_stream_io_t *io);
uint8_t __fastcall__ fn_stream_open(fn_stream_io_t *io);
uint8_t __fastcall__ fn_stream_read(fn_stream_io_t *io);
uint8_t __fastcall__ fn_stream_write(fn_stream_io_t *io);
uint8_t __fastcall__ fn_stream_close(fn_stream_io_t *io);
/* Lobby creator 1/app 1 key in length; returns up to 64 bytes in received. */
uint8_t __fastcall__ fn_stream_appkey(fn_stream_io_t *io);
#endif
#endif
