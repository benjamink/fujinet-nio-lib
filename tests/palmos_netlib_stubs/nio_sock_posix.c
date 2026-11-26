/* nio_sock.h on BSD sockets, for the NioLib Network transport's host test. */
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "nio_sock.h"

int nio_sock_start(NioNet *n) { (void)n; return 0; }
void nio_sock_stop(NioNet *n) { (void)n; }

int nio_sock_tcp_connect(NioNet *n, const char *host, uint16_t port, long *sock)
{
    struct addrinfo hints, *res, *ai;
    char service[8];
    int fd = -1;

    (void)n;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%u", (unsigned)port);
    if (getaddrinfo(host, service, &hints, &res) != 0) return NIO_SOCK_ERROR;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return NIO_SOCK_ERROR;
    *sock = fd;
    return 0;
}

int nio_sock_send(NioNet *n, long sock, const uint8_t *data, uint16_t len)
{
    ssize_t r = send((int)sock, data, len, 0);

    (void)n;
    return r < 0 ? NIO_SOCK_ERROR : (int)r;
}

int nio_sock_recv(NioNet *n, long sock, uint8_t *data, uint16_t max, uint16_t wait_ms)
{
    struct pollfd p;
    ssize_t r;

    (void)n;
    p.fd = (int)sock;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, wait_ms) == 0) return NIO_SOCK_WOULDBLOCK;
    r = recv((int)sock, data, max, 0);
    return r < 0 ? NIO_SOCK_ERROR : (int)r;
}

void nio_sock_close(NioNet *n, long sock) { (void)n; close((int)sock); }

void nio_sock_local_ip(NioNet *n, char *out, uint16_t cap)
{
    (void)n;
    snprintf(out, cap, "127.0.0.1");
}
