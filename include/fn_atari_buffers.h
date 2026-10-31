#ifndef FN_ATARI_BUFFERS_H
#define FN_ATARI_BUFFERS_H

#include "fujinet-nio.h"
#include "fn_protocol.h"

/* Optional compact Atari backend workspace ownership. Define both arrays in
 * exactly one application translation unit, after any desired cc65 segment
 * pragma. Both must be writable, non-overlapping and at least 512 bytes for
 * the lifetime of the library. A reset/retry can require both simultaneously.
 * Without these definitions the library provides its own buffers. */
#define FN_ATARI_DEFINE_PACKET_BUFFERS() \
    uint8_t _fn_req_buf[FN_MAX_PACKET_SIZE]; \
    uint8_t _fn_resp_buf[FN_MAX_PACKET_SIZE]

#endif
