/* NioLib's Network transport: FujiNet NIO's device side, in C, running on the Palm (from fujinet-nio-palmOS). Its
 * host tests are tests/palmos_netlib_host_test.c in this repository.
 *
 * nio_exchange() takes one FujiBus request packet and writes its reply, as a
 * FujiNet on the other end of a serial line would. The devices reach the
 * network through nio_sock.h (NetLib on Palm OS, BSD sockets in host tests).
 * All state lives in one NioState so the core can later sit in a Palm shared
 * library, which has no globals of its own.
 */
#ifndef NIO_H
#define NIO_H

#include <stdint.h>
#include "nio_sock.h"

#define NIO_VERSION_STRING "palmos-0.1"
#define NIO_PROFILE_STRING "palmos-netlib"

/* Wire device ids and status codes, as in fujinet-nio. */
#define NIO_DEV_FUJI    0x70
#define NIO_DEV_WIFI    0xF3
#define NIO_DEV_NETWORK 0xFD
#define NIO_DEV_FILE    0xFE

#define NIO_ST_OK          0
#define NIO_ST_NO_DEVICE   1
#define NIO_ST_INVALID     2
#define NIO_ST_BUSY        3
#define NIO_ST_NOT_READY   4
#define NIO_ST_IO_ERROR    5
#define NIO_ST_TIMEOUT     6
#define NIO_ST_INTERNAL    7
#define NIO_ST_UNSUPPORTED 8

#define NIO_SESSIONS   4
#define NIO_RX_BYTES   1024   /* per session: header parse and read-ahead */
#define NIO_HDR_BYTES  256    /* per session: captured response headers */
#define NIO_WANT_BYTES 96     /* per session: names of headers to capture */
#define NIO_HEADER     6      /* FujiBus header bytes */
#define NIO_REPLY_HEAD 7      /* header plus the status parameter */

typedef struct {
    uint8_t  used;
    uint8_t  http;            /* 0 = tcp stream, 1 = http */
    uint8_t  peer_closed;
    uint8_t  failed;
    uint8_t  headers_done;    /* http: status line and headers parsed */
    uint8_t  has_length;
    uint8_t  head_only;       /* http: HEAD, no body follows */
    uint8_t  skip_line;       /* http: inside a header line longer than rx */
    uint8_t  body_open;       /* http: body of unknown length being gathered in rx */
    uint8_t  pad;
    NioNet  *net;             /* the state's, for the session's socket calls */
    uint16_t handle;
    uint16_t http_status;
    long     sock;
    uint32_t read_cursor;
    uint32_t write_cursor;
    uint32_t content_length;
    uint32_t body_expected;   /* http POST/PUT with a known length: body bytes still to write */
    uint16_t rx_len;
    uint16_t hdr_len;
    uint8_t  rx[NIO_RX_BYTES];
    char     hdr[NIO_HDR_BYTES];
    char     want[NIO_WANT_BYTES];   /* lower-case names, each ending in ':' */
} NioSession;

typedef struct {
    uint8_t    net_up;
    uint8_t    reserved;
    uint16_t   next_handle;
    NioNet     net;
    NioSession sessions[NIO_SESSIONS];
} NioState;

void nio_init(NioState *nio);
/* Closes every session and the network stack. */
void nio_shutdown(NioState *nio);

/* One request in, one reply out. Returns the reply length, or 0 when the
 * request is not a FujiBus packet or the reply buffer is too small. */
uint16_t nio_exchange(NioState *nio, const uint8_t *req, uint16_t req_len,
                      uint8_t *resp, uint16_t resp_cap);

/* Device handlers: payload in, payload out (at out, at most cap bytes);
 * return a status and set *out_len. */
uint8_t nio_network(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                    uint8_t *out, uint16_t cap, uint16_t *out_len);
uint8_t nio_fuji(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                 uint8_t *out, uint16_t cap, uint16_t *out_len);
uint8_t nio_wifi(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                 uint8_t *out, uint16_t cap, uint16_t *out_len);

/* Brings the network stack up on first use. */
uint8_t nio_net_up(NioState *nio);
/* Closes every session and takes the network stack down (NIO Config's Disconnect). */
void nio_net_down(NioState *nio);
/* Sessions open now. */
uint8_t nio_sessions_open(const NioState *nio);

/* Little-endian helpers. */
#define NIO_GET16(p) ((uint16_t)((p)[0] | ((uint16_t)(p)[1] << 8)))
#define NIO_GET32(p) ((uint32_t)NIO_GET16(p) | ((uint32_t)NIO_GET16((p) + 2) << 16))
void nio_put16(uint8_t *p, uint16_t v);
void nio_put32(uint8_t *p, uint32_t v);

#endif
