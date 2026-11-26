/* Host test of NioLib's Network transport core (src/platform/palmos/shared/netlib): FujiBus request packets go
 * through nio_exchange() to the NetworkDevice, which talks over BSD sockets (palmos_netlib_stubs/nio_sock_posix.c) to
 * a scripted server running as a thread on 127.0.0.1. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include "nio.h"

static int failures, checks;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

/* ---- the scripted server ---- */

enum { SRV_REPLY, SRV_ECHO, SRV_HOLD };

typedef struct {
    int mode;
    const char *reply;            /* SRV_REPLY: sent after the request, then the connection is closed */
    int reply_len;
    int lfd;
    uint16_t port;
    volatile int stop;
    int conns;
    char req[4096];               /* the request as received */
    int req_len;
    pthread_t th;
} Srv;

static int want_body(const char *req, int len)
{
    const char *p = strstr(req, "Content-Length: ");
    const char *end = strstr(req, "\r\n\r\n");

    if (!p || !end || p > end) return 0;
    (void)len;
    return atoi(p + 16);
}

static int recv_wait(int fd, char *buf, int max, int ms)
{
    struct pollfd p;

    p.fd = fd;
    p.events = POLLIN;
    if (poll(&p, 1, ms) <= 0) return -1;
    return (int)recv(fd, buf, (size_t)max, 0);
}

static void serve_http(Srv *s, int fd)
{
    int n, total = 0, need = -1;

    for (;;) {
        n = recv_wait(fd, s->req + total, (int)sizeof(s->req) - 1 - total, 2000);
        if (n <= 0) break;
        total += n;
        s->req[total] = 0;
        if (need < 0 && strstr(s->req, "\r\n\r\n"))
            need = (int)(strstr(s->req, "\r\n\r\n") - s->req) + 4 + want_body(s->req, total);
        if (need >= 0 && total >= need) break;
    }
    s->req_len = total;
    if (s->reply_len) send(fd, s->reply, (size_t)s->reply_len, 0);
}

static void serve_echo(Srv *s, int fd)
{
    char buf[512];
    int n;

    while (!s->stop && (n = recv_wait(fd, buf, sizeof(buf), 50)) != 0)
        if (n > 0) send(fd, buf, (size_t)n, 0);
}

static void *srv_thread(void *arg)
{
    Srv *s = arg;
    struct pollfd p;
    int held[16], nheld = 0, fd, i;

    p.fd = s->lfd;
    p.events = POLLIN;
    while (!s->stop) {
        if (poll(&p, 1, 20) <= 0) continue;
        fd = accept(s->lfd, NULL, NULL);
        if (fd < 0) continue;
        s->conns++;
        if (s->mode == SRV_HOLD) {
            if (nheld < 16) held[nheld++] = fd; else close(fd);
            continue;
        }
        if (s->mode == SRV_ECHO) serve_echo(s, fd);
        else serve_http(s, fd);
        close(fd);
        if (s->mode == SRV_REPLY) break;
    }
    for (i = 0; i < nheld; i++) close(held[i]);
    return NULL;
}

static void srv_start(Srv *s, int mode, const char *reply)
{
    struct sockaddr_in a;
    socklen_t l = sizeof(a);

    memset(s, 0, sizeof(*s));
    s->mode = mode;
    s->reply = reply;
    s->reply_len = reply ? (int)strlen(reply) : 0;
    s->lfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7F000001UL);
    if (s->lfd < 0 || bind(s->lfd, (struct sockaddr *)&a, sizeof(a)) || listen(s->lfd, 16) ||
        getsockname(s->lfd, (struct sockaddr *)&a, &l)) {
        perror("server socket");
        exit(2);
    }
    s->port = ntohs(a.sin_port);
    pthread_create(&s->th, NULL, srv_thread, s);
}

/* Stops the server and waits for it, after which the request it saw may be read. */
static void srv_finish(Srv *s)
{
    s->stop = 1;
    pthread_join(s->th, NULL);
    close(s->lfd);
}

/* ---- FujiBus requests ---- */

static uint8_t checksum(const uint8_t *p, uint16_t len)
{
    uint16_t sum = 0, i;

    for (i = 0; i < len; i++) {
        if (i == 4) continue;
        sum = (uint16_t)(sum + p[i]);
        sum = (uint16_t)((sum >> 8) + (sum & 0xFF));
    }
    return (uint8_t)sum;
}

static NioState st;
static uint8_t resp[2048];
static uint16_t resp_len;

/* One exchange; returns the reply's status (the payload is at resp + NIO_REPLY_HEAD). */
static int xchg(uint8_t dev, uint8_t cmd, const uint8_t *pl, uint16_t plen)
{
    uint8_t req[2048];
    uint16_t total = (uint16_t)(NIO_HEADER + plen);

    req[0] = dev;
    req[1] = cmd;
    nio_put16(req + 2, total);
    req[4] = 0;
    req[5] = 0;
    memcpy(req + NIO_HEADER, pl, plen);
    req[4] = checksum(req, total);
    resp_len = nio_exchange(&st, req, total, resp, sizeof(resp));
    CHECK(resp_len >= NIO_REPLY_HEAD);
    if (resp_len < NIO_REPLY_HEAD) return -1;
    CHECK(resp[2] == (uint8_t)resp_len && resp[3] == (uint8_t)(resp_len >> 8));
    CHECK(checksum(resp, resp_len) == resp[4]);
    return resp[6];
}

static uint16_t put_str16(uint8_t *p, const char *s)
{
    uint16_t n = (uint16_t)strlen(s);

    nio_put16(p, n);
    memcpy(p + 2, s, n);
    return (uint16_t)(2 + n);
}

/* An Open payload: method, flags, url, request headers (name, value pairs), body length, response header names. */
static int open_ex(int method, int flags, const char *url, const char *const *hdrs, int nh, uint32_t body,
                   const char *const *want, int nw)
{
    uint8_t pl[1200];
    uint16_t at = 0;
    int i;

    pl[at++] = 1;
    pl[at++] = (uint8_t)method;
    pl[at++] = (uint8_t)flags;
    at = (uint16_t)(at + put_str16(pl + at, url));
    nio_put16(pl + at, (uint16_t)nh);
    at = (uint16_t)(at + 2);
    for (i = 0; i < nh * 2; i++) at = (uint16_t)(at + put_str16(pl + at, hdrs[i]));
    nio_put32(pl + at, body);
    at = (uint16_t)(at + 4);
    nio_put16(pl + at, (uint16_t)nw);
    at = (uint16_t)(at + 2);
    for (i = 0; i < nw; i++) at = (uint16_t)(at + put_str16(pl + at, want[i]));
    return xchg(NIO_DEV_NETWORK, 1, pl, at);
}

static int open_url(int method, const char *url)
{
    return open_ex(method, 0, url, NULL, 0, 0, NULL, 0);
}

static uint16_t handle(void) { return NIO_GET16(resp + NIO_REPLY_HEAD + 4); }

static int handle_cmd(uint8_t cmd, uint16_t h)
{
    uint8_t pl[3];

    pl[0] = 1;
    nio_put16(pl + 1, h);
    return xchg(NIO_DEV_NETWORK, cmd, pl, 3);
}

static int read_at(uint16_t h, uint32_t offset, uint16_t max)
{
    uint8_t pl[9];

    pl[0] = 1;
    nio_put16(pl + 1, h);
    nio_put32(pl + 3, offset);
    nio_put16(pl + 7, max);
    return xchg(NIO_DEV_NETWORK, 2, pl, 9);
}

static int write_at(uint16_t h, uint32_t offset, const char *data, uint16_t n)
{
    uint8_t pl[300];

    pl[0] = 1;
    nio_put16(pl + 1, h);
    nio_put32(pl + 3, offset);
    nio_put16(pl + 7, n);
    memcpy(pl + 9, data, n);
    return xchg(NIO_DEV_NETWORK, 3, pl, (uint16_t)(9 + n));
}

/* Fields of the last Read reply. */
static uint8_t rd_flags(void) { return resp[NIO_REPLY_HEAD + 1]; }
static uint16_t rd_got(void) { return NIO_GET16(resp + NIO_REPLY_HEAD + 10); }
static const char *rd_data(void) { return (const char *)resp + NIO_REPLY_HEAD + 12; }

/* Reads a body to its end (EOF flag), up to cap bytes; returns the Read status that stopped it (0 = EOF). */
static int read_body(uint16_t h, char *buf, int cap, int *total)
{
    uint32_t off = 0;
    int status, tries = 0;

    *total = 0;
    for (;;) {
        status = read_at(h, off, 16);
        if (status == NIO_ST_NOT_READY && ++tries < 20) continue;
        if (status != NIO_ST_OK) return status;
        if (*total + rd_got() < cap) memcpy(buf + *total, rd_data(), rd_got());
        *total += rd_got();
        off += rd_got();
        if (rd_flags() & 0x01) return 0;
    }
}

static void url_for(char *out, size_t cap, const Srv *s, const char *tail)
{
    snprintf(out, cap, "http://127.0.0.1:%u%s", (unsigned)s->port, tail);
}

static int has(const Srv *s, const char *text) { return strstr(s->req, text) != NULL; }

/* ---- tests ---- */

static void test_get_content_length(void)
{
    Srv s;
    char url[128], body[64];
    const char *hdrs[] = {"X-Test", "one", "Accept", "*/*"};
    int total;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 11\r\n\r\nhello world");
    url_for(url, sizeof(url), &s, "/path/x?y=1");
    CHECK(open_ex(1, 0, url, hdrs, 2, 0, NULL, 0) == NIO_ST_OK);
    CHECK(nio_sessions_open(&st) == 1);
    CHECK(read_body(handle(), body, sizeof(body), &total) == 0);
    CHECK(total == 11 && memcmp(body, "hello world", 11) == 0);
    CHECK(handle_cmd(5, handle()) == NIO_ST_OK);
    CHECK(resp[NIO_REPLY_HEAD + 1] & 0x02);                       /* length known */
    CHECK(NIO_GET32(resp + NIO_REPLY_HEAD + 8) == 11);
    CHECK(handle_cmd(4, handle()) == NIO_ST_OK);
    CHECK(nio_sessions_open(&st) == 0);
    srv_finish(&s);
    /* The request line, host, and the headers sent are what the server saw. */
    CHECK(has(&s, "GET /path/x?y=1 HTTP/1.0\r\nHost: 127.0.0.1:"));
    CHECK(has(&s, "\r\nConnection: close\r\n"));
    CHECK(has(&s, "\r\nX-Test: one\r\n"));
    CHECK(has(&s, "\r\nAccept: */*\r\n"));
    CHECK(has(&s, "\r\n\r\n"));
}

static void test_head(void)
{
    Srv s;
    char url[128];
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 1234\r\n\r\n");
    url_for(url, sizeof(url), &s, "/big");
    CHECK(open_url(5, url) == NIO_ST_OK);
    h = handle();
    CHECK(handle_cmd(5, h) == NIO_ST_OK);
    CHECK(NIO_GET16(resp + NIO_REPLY_HEAD + 6) == 200);
    CHECK(NIO_GET32(resp + NIO_REPLY_HEAD + 8) == 1234);
    CHECK(read_at(h, 0, 32) == NIO_ST_OK);
    CHECK(rd_got() == 0 && (rd_flags() & 0x01));
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
    CHECK(strncmp(s.req, "HEAD /big HTTP/1.0\r\n", 20) == 0);
}

static void test_post_known(void)
{
    Srv s;
    char url[128], body[32];
    int total;
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 201 Created\r\nContent-Length: 2\r\n\r\nok");
    url_for(url, sizeof(url), &s, "/post");
    CHECK(open_ex(2, 0, url, NULL, 0, 5, NULL, 0) == NIO_ST_OK);
    h = handle();
    CHECK(resp[NIO_REPLY_HEAD + 1] & 0x02);                       /* a body is expected */
    CHECK(read_at(h, 0, 8) == NIO_ST_NOT_READY);                  /* not before the body is written */
    CHECK(write_at(h, 0, "hello", 5) == NIO_ST_OK);
    CHECK(NIO_GET16(resp + NIO_REPLY_HEAD + 10) == 5);
    CHECK(write_at(h, 5, "x", 1) == NIO_ST_INVALID);              /* more than declared */
    CHECK(read_body(h, body, sizeof(body), &total) == 0 && total == 2 && memcmp(body, "ok", 2) == 0);
    CHECK(handle_cmd(5, h) == NIO_ST_OK);
    CHECK(NIO_GET16(resp + NIO_REPLY_HEAD + 6) == 201);
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
    CHECK(strncmp(s.req, "POST /post HTTP/1.0\r\n", 21) == 0);
    CHECK(has(&s, "\r\nContent-Length: 5\r\n\r\nhello"));
}

static void test_post_unknown(void)
{
    Srv s;
    char url[128], body[32];
    int total;
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 3\r\n\r\nyes");
    url_for(url, sizeof(url), &s, "/up");
    CHECK(open_ex(2, 0x04, url, NULL, 0, 0, NULL, 0) == NIO_ST_OK);   /* OPEN_FLAG_BODY_UNKNOWN */
    h = handle();
    CHECK(read_at(h, 0, 8) == NIO_ST_NOT_READY);
    CHECK(write_at(h, 0, "ab", 2) == NIO_ST_OK);
    CHECK(write_at(h, 2, "cd", 2) == NIO_ST_OK);
    CHECK(write_at(h, 4, "", 0) == NIO_ST_OK);                    /* the commit */
    CHECK(read_body(h, body, sizeof(body), &total) == 0 && total == 3 && memcmp(body, "yes", 3) == 0);
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
    CHECK(has(&s, "\r\nContent-Length: 4\r\n\r\nabcd"));
}

static void test_status_404(void)
{
    Srv s;
    char url[128];
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found");
    url_for(url, sizeof(url), &s, "/missing");
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(handle_cmd(5, h) == NIO_ST_OK);
    CHECK(NIO_GET16(resp + NIO_REPLY_HEAD + 6) == 404);
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

static void test_close_delimited(void)
{
    Srv s;
    char url[128], body[64];
    int total;
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nServer: t\r\n\r\nbody without a length");
    url_for(url, sizeof(url), &s, "/");
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(read_body(h, body, sizeof(body), &total) == 0);          /* ends with EOF, not an error */
    CHECK(total == 21 && memcmp(body, "body without a length", 21) == 0);
    CHECK(handle_cmd(5, h) == NIO_ST_OK);
    CHECK(!(resp[NIO_REPLY_HEAD + 1] & 0x02));                    /* no length */
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

static void test_long_header_line(void)
{
    Srv s;
    char reply[2400], url[128], body[16];
    int total, n;
    uint16_t h;

    n = snprintf(reply, sizeof(reply), "HTTP/1.0 200 OK\r\nX-Long: ");
    memset(reply + n, 'a', 1800);
    n += 1800;
    snprintf(reply + n, sizeof(reply) - (size_t)n, "\r\nContent-Length: 5\r\n\r\nhello");
    srv_start(&s, SRV_REPLY, reply);
    url_for(url, sizeof(url), &s, "/long");
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(handle_cmd(5, h) == NIO_ST_OK);                         /* the long line is dropped, the rest parsed */
    CHECK(NIO_GET16(resp + NIO_REPLY_HEAD + 6) == 200);
    CHECK(NIO_GET32(resp + NIO_REPLY_HEAD + 8) == 5);
    CHECK(read_body(h, body, sizeof(body), &total) == 0 && total == 5 && memcmp(body, "hello", 5) == 0);
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

static void test_query_without_path(void)
{
    Srv s;
    char url[128];

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
    url_for(url, sizeof(url), &s, "?x=1");
    CHECK(open_url(1, url) == NIO_ST_OK);
    CHECK(handle_cmd(5, handle()) == NIO_ST_OK);
    CHECK(handle_cmd(4, handle()) == NIO_ST_OK);
    srv_finish(&s);
    CHECK(strncmp(s.req, "GET /?x=1 HTTP/1.0\r\n", 20) == 0);
}

static void test_response_headers(void)
{
    Srv s;
    char url[128];
    const char *want[] = {"Content-Type", "x-foo"};
    uint16_t hl;

    srv_start(&s, SRV_REPLY,
              "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nX-Other: no\r\nX-Foo: bar\r\nContent-Length: 0\r\n\r\n");
    url_for(url, sizeof(url), &s, "/h");
    CHECK(open_ex(1, 0, url, NULL, 0, 0, want, 2) == NIO_ST_OK);
    CHECK(handle_cmd(5, handle()) == NIO_ST_OK);
    CHECK(resp[NIO_REPLY_HEAD + 1] & 0x01);                       /* headers present */
    hl = NIO_GET16(resp + NIO_REPLY_HEAD + 16);
    CHECK(hl == strlen("Content-Type: text/plain\r\nX-Foo: bar\r\n"));
    CHECK(hl && memcmp(resp + NIO_REPLY_HEAD + 18, "Content-Type: text/plain\r\nX-Foo: bar\r\n", hl) == 0);
    CHECK(handle_cmd(4, handle()) == NIO_ST_OK);
    srv_finish(&s);
}

/* Control characters would split the request line or a header line: all refused before anything is sent. */
static void test_injection_rejected(void)
{
    Srv s;
    char url[200];
    const char *bad_name[] = {"X-A\r\nX-Injected", "v", "", "v", "Bad Name", "v", "Bad:Name", "v", "X\x7f", "v", "X\tY", "v"};
    const char *bad_value[] = {"X-A", "v\r\nX-Injected: yes", "X-A", "v\nx", "X-A", "v\x01", "X-A", "v\x7f"};
    int i;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
    url_for(url, sizeof(url), &s, "/a\r\nX-Injected: yes");
    CHECK(open_url(1, url) == NIO_ST_INVALID);
    url_for(url, sizeof(url), &s, "/a b");
    CHECK(open_url(1, url) == NIO_ST_INVALID);
    url_for(url, sizeof(url), &s, "/a\x7f");
    CHECK(open_url(1, url) == NIO_ST_INVALID);
    url_for(url, sizeof(url), &s, "/a\x01");
    CHECK(open_url(1, url) == NIO_ST_INVALID);
    url_for(url, sizeof(url), &s, "/ok");
    for (i = 0; i < 6; i++) CHECK(open_ex(1, 0, url, bad_name + 2 * i, 1, 0, NULL, 0) == NIO_ST_INVALID);
    for (i = 0; i < 4; i++) CHECK(open_ex(1, 0, url, bad_value + 2 * i, 1, 0, NULL, 0) == NIO_ST_INVALID);
    CHECK(nio_sessions_open(&st) == 0);
    srv_finish(&s);
    CHECK(s.conns == 0);                                          /* no socket was ever opened */
    CHECK(!has(&s, "Injected"));
}

static void test_tab_in_value_allowed(void)
{
    Srv s;
    char url[128];
    const char *hdrs[] = {"X-Tab", "a\tb"};

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
    url_for(url, sizeof(url), &s, "/t");
    CHECK(open_ex(1, 0, url, hdrs, 1, 0, NULL, 0) == NIO_ST_OK);
    CHECK(handle_cmd(5, handle()) == NIO_ST_OK);
    CHECK(handle_cmd(4, handle()) == NIO_ST_OK);
    srv_finish(&s);
    CHECK(has(&s, "\r\nX-Tab: a\tb\r\n"));
}

/* The server declares 100 bytes, sends 10 and closes. */
static void test_truncated_body(void)
{
    Srv s;
    char url[128];
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 100\r\n\r\n0123456789");
    url_for(url, sizeof(url), &s, "/short");
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(read_at(h, 0, 64) == NIO_ST_OK);
    CHECK(rd_got() == 10 && memcmp(rd_data(), "0123456789", 10) == 0);
    CHECK(!(rd_flags() & 0x01));                                  /* short, not at its end */
    CHECK(read_at(h, 10, 64) == NIO_ST_IO_ERROR);
    CHECK(read_at(h, 10, 64) == NIO_ST_IO_ERROR);                 /* and stays one */
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

/* A close with no Content-Length is the body's end, even when the first Read also saw the close. */
static void test_close_delimited_eof(void)
{
    Srv s;
    char url[128];
    uint16_t h;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\n\r\n0123456789");
    url_for(url, sizeof(url), &s, "/c");
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(read_at(h, 0, 64) == NIO_ST_OK);
    CHECK(rd_got() == 10);
    if (!(rd_flags() & 0x01)) {
        CHECK(read_at(h, 10, 64) == NIO_ST_OK);
        CHECK(rd_got() == 0 && (rd_flags() & 0x01));
    }
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

static void test_tcp_echo(void)
{
    Srv s;
    char url[64], got[32];
    int status, tries = 0, total = 0;
    uint16_t h;

    srv_start(&s, SRV_ECHO, NULL);
    snprintf(url, sizeof(url), "tcp://127.0.0.1:%u", (unsigned)s.port);
    CHECK(open_url(1, url) == NIO_ST_OK);
    h = handle();
    CHECK(write_at(h, 0, "ping pong", 9) == NIO_ST_OK);
    while (total < 9 && tries++ < 100) {
        status = read_at(h, (uint32_t)total, 32);
        if (status == NIO_ST_NOT_READY) { struct timespec ts = { 0, 10000000L }; nanosleep(&ts, NULL); continue; }
        CHECK(status == NIO_ST_OK);
        if (status != NIO_ST_OK) break;
        memcpy(got + total, rd_data(), rd_got());
        total += rd_got();
    }
    CHECK(total == 9 && memcmp(got, "ping pong", 9) == 0);
    CHECK(handle_cmd(4, h) == NIO_ST_OK);
    srv_finish(&s);
}

static void test_sessions_full(void)
{
    Srv s;
    char url[64];
    uint16_t hs[NIO_SESSIONS];
    int i;

    srv_start(&s, SRV_HOLD, NULL);
    snprintf(url, sizeof(url), "tcp://127.0.0.1:%u", (unsigned)s.port);
    for (i = 0; i < NIO_SESSIONS; i++) {
        CHECK(open_url(1, url) == NIO_ST_OK);
        hs[i] = handle();
    }
    CHECK(nio_sessions_open(&st) == NIO_SESSIONS);
    CHECK(open_url(1, url) == NIO_ST_BUSY);
    CHECK(handle_cmd(4, hs[1]) == NIO_ST_OK);                     /* Close frees one */
    CHECK(nio_sessions_open(&st) == NIO_SESSIONS - 1);
    CHECK(handle_cmd(4, hs[1]) == NIO_ST_INVALID);                /* and the handle is gone */
    CHECK(open_url(1, url) == NIO_ST_OK);
    CHECK(nio_sessions_open(&st) == NIO_SESSIONS);
    nio_shutdown(&st);
    CHECK(nio_sessions_open(&st) == 0);
    srv_finish(&s);
}

/* Truncated means the caller's buffer was filled: the rest of a short body read into a bigger buffer is EOF only. */
static void test_truncated_flag(void)
{
    Srv s;
    char url[128];
    int st_;
    int tries;

    srv_start(&s, SRV_REPLY, "HTTP/1.0 200 OK\r\nContent-Length: 10\r\n\r\n0123456789");
    url_for(url, sizeof(url), &s, "/t");
    CHECK(open_url(1, url) == NIO_ST_OK);
    for (tries = 0; (st_ = read_at(handle(), 0, 4)) == NIO_ST_NOT_READY && tries < 20; tries++) {}
    CHECK(st_ == NIO_ST_OK && rd_got() == 4 && rd_flags() == 0x02);
    for (tries = 0; (st_ = read_at(handle(), 4, 256)) == NIO_ST_NOT_READY && tries < 20; tries++) {}
    CHECK(st_ == NIO_ST_OK && rd_got() == 6 && rd_flags() == 0x01);
    CHECK(memcmp(rd_data(), "456789", 6) == 0);
    CHECK(handle_cmd(4, handle()) == NIO_ST_OK);
    srv_finish(&s);
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    nio_init(&st);
    test_get_content_length();
    test_head();
    test_post_known();
    test_post_unknown();
    test_status_404();
    test_close_delimited();
    test_long_header_line();
    test_query_without_path();
    test_response_headers();
    test_injection_rejected();
    test_tab_in_value_allowed();
    test_truncated_body();
    test_truncated_flag();
    test_close_delimited_eof();
    test_tcp_echo();
    test_sessions_full();
    nio_shutdown(&st);
    printf("palmos netlib host test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
