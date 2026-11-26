/* The network stack under the NIO core: NetLib on Palm OS, BSD sockets on a
 * host. Blocking calls with timeouts; the core is single-threaded. The stack's
 * state is a NioNet in the caller's NioState, not globals: on the Palm the core
 * runs in a shared library, which has none. */
#ifndef NIO_SOCK_H
#define NIO_SOCK_H

#include <stdint.h>

typedef struct {
    uint16_t ref;             /* Palm: Net.lib's reference number */
    uint8_t  open;            /* Palm: NetLibOpen succeeded */
    uint8_t  pad;             /* set: stop takes the link down at once */
    uint32_t bytesIn;         /* counted by the core, for whoever asks (NIO Config) */
    uint32_t bytesOut;
    uint16_t connects;
    uint16_t failures;
} NioNet;

#define NIO_SOCK_ERROR      (-1)
#define NIO_SOCK_WOULDBLOCK (-2)

/* 0 on success. start may bring a PPP link up and take a while. */
int  nio_sock_start(NioNet *n);
void nio_sock_stop(NioNet *n);

/* Resolves host and connects. 0 on success with *sock set. */
int  nio_sock_tcp_connect(NioNet *n, const char *host, uint16_t port, long *sock);
/* Bytes sent, or NIO_SOCK_ERROR. */
int  nio_sock_send(NioNet *n, long sock, const uint8_t *data, uint16_t len);
/* Bytes read, 0 once the peer has closed, NIO_SOCK_WOULDBLOCK when nothing
 * came within wait_ms, or NIO_SOCK_ERROR. */
int  nio_sock_recv(NioNet *n, long sock, uint8_t *data, uint16_t max, uint16_t wait_ms);
void nio_sock_close(NioNet *n, long sock);
/* Dotted address of this machine, "" when unknown. */
void nio_sock_local_ip(NioNet *n, char *out, uint16_t cap);

#endif
